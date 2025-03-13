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

#include <l4/backtracer/btb_control.h>

#include "compress.cpp"

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

	// pointer is redirected if we use compression, length also changes then.
	unsigned long * actual_result_buffer = (unsigned long *) kumem;
	unsigned long actual_result_words;

	compression_header_t * compression_header_1 = (compression_header_t *) kumem;

	const unsigned long header_capacity_in_words = (
		try_compress
		? (sizeof(compression_header_t) - 1) / sizeof(unsigned long) + 1
		: 0
	);
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

	unsigned long dictionary_and_compressed [header_capacity_in_words + returned_words];
	if (returned_words && try_compress) {
		// we will try to compress into this data buffer,
		// if dictionary + compressed data don't fit, it's not worth it.
		printf(
			"trying to compress\n"
			"    btb  %16p (cap %8lx w, len %8lx w)\n",
			buffer, buffer_capacity_in_words, returned_words
			// continued inside compress_smart, different vars are visible inside or outside
		);
		ssize_t compressed_in_words = compress_smart(
			dictionary_and_compressed,
			returned_words + header_capacity_in_words,
			buffer,
			returned_words,
			compression_header_1
		);
		if (compressed_in_words < 0) {
			// couldn't compress into the given compressed buffer,
			// leave actual_result_buffer where it is.
			actual_result_words = header_capacity_in_words + returned_words;
		} else {
			actual_result_buffer = &dictionary_and_compressed[0];
			actual_result_words = compressed_in_words;
		}
	}

	if (returned_words) {
		printf(
			"printing %16p (len %8lx w =    %8lx B)\n",
			actual_result_buffer, actual_result_words, actual_result_words * sizeof(unsigned long)
		);
		print_backtrace_buffer_section(actual_result_buffer, actual_result_words);
	}

	return remaining_words;
}

