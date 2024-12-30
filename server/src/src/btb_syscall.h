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
#include <l4/re/c/util/kumem_alloc.h>
#include <sys/mman.h>

#include "compress.cpp"

enum backtrace_buffer_control {
	BTB_CONTROL_START        = (1 << 0),
	BTB_CONTROL_STOP         = (1 << 1),
	BTB_CONTROL_RESET        = (1 << 2),
	BTB_CONTROL_SET_TIMESTEP = (1 << 3),
	BTB_CONTROL_GET_TIMESTEP = (1 << 4),
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

static inline
l4_msgtag_t
l4_debugger_backtracing_control_2(
	l4_cap_idx_t cap,
	unsigned long flags,
	unsigned long arg
) L4_NOTHROW {
	l4_utcb_t * utcb = l4_utcb();
	l4_utcb_mr_u(utcb)->mr[0] = L4_DEBUGGER_BTB_CONTROL;
	l4_utcb_mr_u(utcb)->mr[1] = flags;
	l4_utcb_mr_u(utcb)->mr[2] = arg;
	l4_msgtag_t tag = l4_msgtag(0, 3, 0, 0);

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

static inline l4_msgtag_t
l4_debugger_backtracing_set_timestep(l4_cap_idx_t cap, unsigned long time_step) L4_NOTHROW {
	return l4_debugger_backtracing_control_2(cap, BTB_CONTROL_SET_TIMESTEP, time_step);
}

static inline l4_msgtag_t
l4_debugger_backtracing_get_timestep(l4_cap_idx_t cap, unsigned long * time_step) L4_NOTHROW {
	l4_msgtag_t syscall_result = l4_debugger_backtracing_control(cap, BTB_CONTROL_GET_TIMESTEP);
	*time_step = l4_utcb_mr()->mr[1];
	return syscall_result;
}

static inline
l4_msgtag_t
l4_debugger_get_backtrace_buffer_section(
	l4_cap_idx_t cap,
	unsigned long * kumem,
	unsigned long kumem_capacity_in_words,
	unsigned long buffer_offset_in_words,
	unsigned long flags,
	unsigned long * returned_words,
	unsigned long * remaining_words
) L4_NOTHROW {
	l4_utcb_t * utcb = l4_utcb();
	l4_utcb_mr_u(utcb)->mr[0] = L4_DEBUGGER_GET_BTB_SECTION;
	l4_utcb_mr_u(utcb)->mr[1] = (unsigned long) kumem;
	l4_utcb_mr_u(utcb)->mr[2] = kumem_capacity_in_words;
	l4_utcb_mr_u(utcb)->mr[3] = buffer_offset_in_words;
	l4_utcb_mr_u(utcb)->mr[4] = flags;
	l4_msgtag_t tag = l4_msgtag(0, 5, 0, 0);

	print_utcb("=*>", utcb, tag);

	l4_msgtag_t syscall_result = l4_invoke_debugger(cap, tag, utcb);

	print_utcb("<*=", utcb, syscall_result);

	if (l4_msgtag_has_error(syscall_result)) {
		*returned_words = 0;
		*remaining_words = 0;
		printf("XXX syscall returned error!\n");
		return syscall_result;
	}

	*returned_words  = l4_utcb_mr_u(utcb)->mr[0];
	*remaining_words = l4_utcb_mr_u(utcb)->mr[1];

	return syscall_result;
}

static const bool print_xor_blocks_for_debugging = false;
static inline
void print_backtrace_buffer_section (const unsigned long * buffer, unsigned long words) {
	unsigned long count_full_blocks = words / block_data_capacity_in_words;
	printf(
		"--> btbs: %ld bytes, %ld words, %ld full blocks\n",
		words * sizeof(unsigned long), words, count_full_blocks
	);

	// initialize the xor block with the first amount of data
	block_t xor_block = make_block (
		buffer,
		(block_data_capacity_in_words < words ? block_data_capacity_in_words : words),
		0
	);

	// print while it still behaves like the first block of data.
	print_block(&xor_block, 0);

	xor_block.flags |= BLOCK_REDUNDANCY;

	for (unsigned long b = 1; b < count_full_blocks; b++) {
		block_t block = make_block(
			buffer + b * block_data_capacity_in_words,
			block_data_capacity_in_words,
			0
		);

		print_block(&block, 0);
		xor_blocks(&xor_block, &block);
		if (print_xor_blocks_for_debugging)
			print_block(&xor_block, "XOR");
	}

	unsigned long remainder = words % block_data_capacity_in_words;
	if (count_full_blocks > 0 && remainder) {
		block_t block = make_block(
			buffer + words - remainder,
			remainder,
			0
		);

		print_block(&block, 0);
		xor_blocks(&xor_block, &block);
	}

	print_block(&xor_block, 0);
}

static inline
unsigned long
export_backtrace_buffer_section (l4_cap_idx_t cap, bool full_section_only, bool try_compress) {
	const unsigned kumem_page_order = 3;
	const unsigned kumem_capacity_in_pages   = (1 << kumem_page_order);
	const unsigned kumem_capacity_in_kibytes = kumem_capacity_in_pages * 4;
	const unsigned kumem_capacity_in_bytes   = kumem_capacity_in_kibytes << 10;
	const unsigned kumem_capacity_in_words   = kumem_capacity_in_bytes / sizeof(unsigned long);
	static l4_addr_t kumem = 0;
	if (!kumem) {
		// TODO: this allocation is never freed. this makes sense,
		// because it basically has life-of-the-process lifetime.
		if (l4re_util_kumem_alloc(&kumem, kumem_page_order, L4_BASE_TASK_CAP, l4re_env()->rm)) {
			printf("!!! could not allocate %d kiB kumem!!!\n", kumem_capacity_in_kibytes);
		}
		printf("successfully allocated %d kiB kumem at %p\n", kumem_capacity_in_kibytes, (void *) kumem);
	}

	// pointer is redicrected if we use compression.
	unsigned long * actual_result_buffer = (unsigned long *) kumem;
	unsigned long actual_result_words;

	compression_header_t * compression_header_1 = (compression_header_t *) kumem;

	const unsigned long header_capacity_in_words = (sizeof(compression_header_t) - 1) / sizeof(unsigned long) + 1;
	const unsigned long buffer_capacity_in_words = kumem_capacity_in_words - header_capacity_in_words;

	unsigned long * buffer = ((unsigned long *) kumem) + header_capacity_in_words;
	unsigned long returned_words;
	unsigned long remaining_words;
	l4_debugger_get_backtrace_buffer_section(
		cap,
		(unsigned long *) kumem,
		kumem_capacity_in_words,
		header_capacity_in_words, // at what offset data should be copied within kumem
		(full_section_only ? FULL_SECTION_ONLY : 0),
		&returned_words,
		&remaining_words
	);

	if (try_compress) {
		// we will try to compress into this data buffer,
		// if dictionary + compressed data don't fit it's not worth it.
		unsigned long dictionary_and_compressed [returned_words + header_capacity_in_words];
		compression_header_t * compression_header_2 = (compression_header_t *) &dictionary_and_compressed[0];
		unsigned long * dictionary = &dictionary_and_compressed[header_capacity_in_words];
		unsigned char * compressed = (unsigned char *) (dictionary + dictionary_capacity);
		const unsigned long compressed_capacity = (returned_words - dictionary_capacity) * sizeof(unsigned long);

		printf(
			"trying to compress\n"
			"    btb  %16p (cap %8ld w, len %ld w)\n"
			"    into %16p (cap %8ld w = %ld B),\n"
			"    dict %16p (cap %8ld w)\n",
			buffer, buffer_capacity_in_words, returned_words,
			compressed, compressed_capacity / sizeof(unsigned long), compressed_capacity,
			dictionary, dictionary_capacity
		);
		ssize_t compressed_bytes = compress_c(
			compressed,
			compressed_capacity,
			buffer,
			returned_words,
			dictionary,
			dictionary_capacity
		);
		compression_header_t * compression_header;
		if (compressed_bytes < 0) {
			// couldn't compress into the given compressed buffer,
			// leave actual_result_buffer where it is.
			actual_result_words = header_capacity_in_words + returned_words;
			// write the header struct it points to
			compression_header_1->is_compressed = false;
			compression_header_1->dictionary_length = 0;
			compression_header_1->dictionary_offset = header_capacity_in_words;
			compression_header_1->data_length_in_bytes = returned_words * sizeof(unsigned long);
			compression_header = compression_header_1;
		} else {
			unsigned long compressed_in_words = (compressed_bytes - 1) / sizeof(unsigned long) + 1;
			actual_result_buffer = &dictionary_and_compressed[0];
			actual_result_words = header_capacity_in_words + dictionary_capacity + compressed_in_words;
			compression_header_2->is_compressed = true;
			compression_header_2->dictionary_length = dictionary_capacity;
			compression_header_2->dictionary_offset = header_capacity_in_words;
			compression_header_2->data_length_in_bytes = compressed_bytes;
			compression_header = compression_header_2;
		}
		printf(
			"compressed: %s,\n"
			"    dict %16p (len %ld w = %ld B),\n"
			"    data %16p (len %ld B => %ld w)\n",
			compression_header->is_compressed ? "True" : "False",
			actual_result_buffer + compression_header->dictionary_offset,
			compression_header->dictionary_length,
			actual_result_buffer + compression_header->dictionary_offset + compression_header->dictionary_length,
			compression_header->data_length_in_bytes,
			(compression_header->data_length_in_bytes - 1) / sizeof(unsigned long) + 1
		);
	}

	if (returned_words) {
		printf(
			"printing buffer %16p, len %ld w = %ld B\n",
			actual_result_buffer, actual_result_words, actual_result_words * sizeof(unsigned long)
		);
		print_backtrace_buffer_section(actual_result_buffer, actual_result_words);
	}

	return remaining_words;
}

