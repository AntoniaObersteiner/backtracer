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
#include <sys/mman.h>
#include <l4/sys/kip.h>
#include <l4/sys/debugger.h>
#include <l4/re/env.h>
#include <l4/sys/linkage.h>
#include <l4/sys/irq.h>

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
	l4_utcb_mr_u(utcb)->mr[1] = buffer;
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

#define BLOCK_SIZE 1024
#define BLOCK_DATA_SIZE (BLOCK_SIZE - 3 * sizeof(unsigned long))
const unsigned long block_data_capacity_in_bytes = BLOCK_DATA_SIZE;
const unsigned long block_data_capacity_in_words = block_data_capacity_in_bytes / sizeof(unsigned long);

enum block_flags_t {
	BLOCK_REDUNDANCY = (1UL << 0),
};

typedef struct block_t_struct {
	unsigned long id;
	unsigned long data_length_in_words;
	unsigned long flags;
	unsigned long reserved;
	unsigned long data [BLOCK_DATA_SIZE / sizeof(unsigned long)];
} block_t;
static_assert (
	sizeof(block_t) == BLOCK_SIZE,
	"block_t is not of the right size. please adjust block_data_capacity_in_bytes"
);

block_t make_block(
	unsigned long * data,
	unsigned long data_length_in_words,
	unsigned long flags
) {
	static unsigned long id = 0;

	block_t block;

	block.id = id;
	block.data_length_in_words = data_length_in_words;
	block.flags = flags;

	if (data_length_in_words > block_data_capacity_in_words) {
		data_length_in_words = block_data_capacity_in_words;
	}

	unsigned long i;
	for (i = 0; i < data_length_in_words; i++) {
		block.data[i] = data[i];
	}
	for (; i < block_data_capacity_in_words; i++) {
		block.data[i] = 0;
	}

	id ++;
	return block;
}

void xor_blocks(block_t * target, const block_t * to_add) {
	unsigned long min_len = target->data_length_in_words;
	if (target->data_length_in_words < min_len) {
		min_len = target->data_length_in_words;
	}
	for (unsigned long i = 0; i < min_len; i++) {
		target->data[i] = target->data[i] | to_add->data[i];
	}
}

void print_block(const block_t * block) {
	const unsigned word_columns = 4;
	unsigned long * raw_block = (unsigned long *) block;
	for (unsigned w = 0; w < sizeof(block_t) / sizeof(unsigned long); w++) {
		if (w % word_columns == 0)
			printf(">=< %3d: ", w);

		printf("%16lx ", raw_block[w]);
		if (w % word_columns == word_columns - 1)
			printf("\n");
	}

	printf("\n");
}
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
	print_block(&xor_block);

	xor_block.flags |= BLOCK_REDUNDANCY;

	for (unsigned long b = 1; b < size_in_blocks; b++) {
		block_t block = make_block(
			buffer + b * block_data_capacity_in_words,
			block_data_capacity_in_words,
			0
		);

		print_block(&block);
		xor_blocks(&xor_block, &block);
	}

	unsigned long remainder = words % block_data_capacity_in_words;
	if (size_in_blocks > 0 && remainder) {
		block_t block = make_block(
			buffer + words - remainder,
			remainder,
			0
		);

		print_block(&block);
		xor_blocks(&xor_block, &block);
	}

	print_block(&xor_block);
}

enum backtrace_buffer_protocol {
	FULL_SECTION_ONLY = 1,
};

unsigned long export_backtrace_buffer_section (l4_cap_idx_t cap, bool full_section_only) {
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
	l4_msgtag_t syscall_result = l4_debugger_get_backtrace_buffer_section(
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

int main(void) {
	l4_cap_idx_t dbg_cap = L4_BASE_DEBUGGER_CAP;
	bool is_valid = l4_is_valid_cap(dbg_cap) > 0;
	printf(">>> dbg_cap %ld is %svalid <<<\n", dbg_cap, is_valid ? "" : "not ");

	// TODO: wait for some event? nope, do than in post-processing.

	// 977 * 2^10 us ~= 1000 ms
	unsigned long mantissa = 977;
	unsigned long exponent = 10;
	unsigned long timeout_us = mantissa * (1 << exponent);
	l4_timeout_t timeout = l4_ipc_timeout(
		mantissa, exponent,
		mantissa, exponent
	);

	for (int i = 0; i < 10; i++) {
		// TODO: we probably want to wait for a complete page, but not busily
		// except that, at the end, we want to get any remainder out.

		unsigned long remaining_words = export_backtrace_buffer_section(dbg_cap, true);

		if (remaining_words == 0) {
			printf(
				"wait for results with timeout %ld us == %f ms...\n",
				timeout_us, (double) timeout_us / 1000.0
			);
			// TODO: replace with waiting on BTB-IRQ-cap that we get from dbg_cap.
			// TODO: alternatively: l4_thread_switch(other thread among traced)
			// pseudo-sleep
			l4_msgtag_t sleep_result = l4_irq_receive(dbg_cap, timeout);
			if (l4_msgtag_has_error(sleep_result)) {
				printf("sleep error!\n");
			}
		}
	}
	// export remaining non-full section
	unsigned long remaining_words = export_backtrace_buffer_section(dbg_cap, false);
}
