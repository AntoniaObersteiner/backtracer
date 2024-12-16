/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Frank Mehnert <fm3@os.inf.tu-dresden.de>,
 *               Lukas Grützmacher <lg2@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <l4/re/env.h>
#include <l4/sys/debugger.h>
#include <l4/sys/irq.h>
#include <l4/sys/linkage.h>
#include <l4/sys/types.h>
#include <sys/mman.h>

enum backtrace_buffer_control {
	BTB_CONTROL_START = (1 << 0),
	BTB_CONTROL_STOP  = (1 << 1),
	BTB_CONTROL_RESET = (1 << 2),
};

enum backtrace_buffer_protocol {
	FULL_SECTION_ONLY = 1,
};

static inline
unsigned long print_utcb(
	const char * prefix,
	l4_utcb_t * utcb,
	l4_msgtag_t tag
) {
	unsigned long words = l4_msgtag_words(tag);
	printf(
		"%s l4_msgtag (%ld, %d, %d, %d) \n",
		prefix,
		l4_msgtag_label(tag),
		l4_msgtag_words(tag),
		l4_msgtag_items(tag),
		l4_msgtag_flags(tag)
	);

  printf("%s [", prefix);
	for (unsigned i = 0; i < words; i++) {
		printf("%d: %ld%s", i, l4_utcb_mr_u(utcb)->mr[i], (i < words - 1) ? ", " : "");
	}
  printf("]\n");

	return words;
}

static inline
l4_msgtag_t
l4_debugger_backtracing_control(
	l4_cap_idx_t cap,
	unsigned long flags
) L4_NOTHROW {
	l4_utcb_t * utcb = l4_utcb();
	l4_utcb_mr_u(utcb)->mr[0] = L4_DEBUGGER_BTB_CONTROL;
	l4_utcb_mr_u(utcb)->mr[1] = flags;
	l4_msgtag_t tag = l4_msgtag(0, 2, 0, 0);

	print_utcb("=>>", utcb, tag);

	l4_msgtag_t syscall_result = l4_invoke_debugger(cap, tag, utcb);

	print_utcb("<<=", utcb, syscall_result);

	return syscall_result;
}

static inline l4_msgtag_t
l4_debugger_backtracing_start(l4_cap_idx_t cap) L4_NOTHROW {
	return l4_debugger_backtracing_control(cap, BTB_CONTROL_START);
}

static inline l4_msgtag_t
l4_debugger_backtracing_stop(l4_cap_idx_t cap) L4_NOTHROW {
	return l4_debugger_backtracing_control(cap, BTB_CONTROL_STOP);
}

static inline l4_msgtag_t
l4_debugger_backtracing_reset(l4_cap_idx_t cap) L4_NOTHROW {
	return l4_debugger_backtracing_control(cap, BTB_CONTROL_RESET);
}

static inline
l4_msgtag_t
l4_debugger_get_backtrace_buffer_section(
	l4_cap_idx_t cap,
	unsigned long * buffer,
	unsigned long buffer_capacity_in_bytes,
	unsigned long flags,
	unsigned long * returned_words,
	unsigned long * remaining_words
) L4_NOTHROW {
	l4_utcb_t * utcb = l4_utcb();
	l4_utcb_mr_u(utcb)->mr[0] = L4_DEBUGGER_GET_BTB_SECTION;
	l4_utcb_mr_u(utcb)->mr[1] = (unsigned long) buffer;
	l4_utcb_mr_u(utcb)->mr[2] = buffer_capacity_in_bytes;
	l4_utcb_mr_u(utcb)->mr[3] = flags;
	l4_msgtag_t tag = l4_msgtag(0, 4, 0, 0);

	print_utcb("=>>", utcb, tag);

	l4_msgtag_t syscall_result = l4_invoke_debugger(cap, tag, utcb);

	print_utcb("<<=", utcb, syscall_result);

	if (l4_msgtag_has_error(syscall_result)) {
		*returned_words = 0;
		*remaining_words = 0;
		return syscall_result;
	}

	*returned_words  = l4_utcb_mr_u(utcb)->mr[0];
	*remaining_words = l4_utcb_mr_u(utcb)->mr[1];

	return syscall_result;
}

static inline
void print_backtrace_buffer_section (const unsigned long * buffer, unsigned long words) {
	unsigned long size_in_blocks = words / block_data_capacity_in_words;
	printf("--> btbs: %ld bytes, %ld words, %ld blocks\n", words * sizeof(unsigned long), words, size_in_blocks);

	// initialize the xor block with the first amount of data
	block_t xor_block = make_block (
		buffer,
		(block_data_capacity_in_words < words ? block_data_capacity_in_words : words),
		0
	);

	// print while it still behaves like the first block of data.
	print_block(&xor_block, 0, true);

	xor_block.flags |= BLOCK_REDUNDANCY;

	for (unsigned long b = 1; b < size_in_blocks; b++) {
		block_t block = make_block(
			buffer + b * block_data_capacity_in_words,
			block_data_capacity_in_words,
			0
		);

		print_block(&block, 0, true);
		xor_blocks(&xor_block, &block);
		print_block(&xor_block, "XOR", true);
	}

	unsigned long remainder = words % block_data_capacity_in_words;
	if (size_in_blocks > 0 && remainder) {
		block_t block = make_block(
			buffer + words - remainder,
			remainder,
			0
		);

		print_block(&block, 0, true);
		xor_blocks(&xor_block, &block);
	}

	print_block(&xor_block, 0, true);
}

static inline
unsigned long
export_backtrace_buffer_section (l4_cap_idx_t cap, bool full_section_only) {
	const unsigned kumem_page_order = 3;
	const unsigned kumem_capacity_in_bytes = (1 << (kumem_page_order + 10));
	static l4_addr_t kumem = 0;
	if (!kumem) {
		// TODO: this allocation is never freed. this makes sense,
		// because it basically has life-of-the-process lifetime.
		if (l4re_util_kumem_alloc(&kumem, kumem_page_order, L4_BASE_TASK_CAP, l4re_env()->rm)) {
			printf("!!! could not allocate %d kiB kumem!!!\n", 1 << kumem_page_order);
		}
		printf("successfully allocated %d kiB kumem at %p\n", 1 << kumem_page_order, (void *) kumem);
	}

	unsigned long * buffer = (unsigned long *) kumem;
	unsigned long returned_words;
	unsigned long remaining_words;
	l4_debugger_get_backtrace_buffer_section(
		cap,
		buffer,
		kumem_capacity_in_bytes,
		(full_section_only ? FULL_SECTION_ONLY : 0),
		&returned_words,
		&remaining_words
	);

	if (returned_words)
		print_backtrace_buffer_section(buffer, returned_words);

	return remaining_words;
}

