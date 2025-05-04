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
#pragma once
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
#include "block.h"
// TODO: maybe remove dependency on measure*.h*
#include "measure_defaults.h"

#define backtrace_buffer_control_count 8
enum backtrace_buffer_control {
	BTB_CONTROL_START        = (1 << 0),
	BTB_CONTROL_STOP         = (1 << 1),
	BTB_CONTROL_RESET        = (1 << 2),
	BTB_CONTROL_SET_TIMESTEP = (1 << 3),
	BTB_CONTROL_GET_TIMESTEP = (1 << 4),
	BTB_CONTROL_IS_RUNNING   = (1 << 5),
	BTB_CONTROL_WRITE_STATS  = (1 << 6),
	BTB_CONTROL_NO_ENTRY     = (1 << 7),
};

int control_to_int (const enum backtrace_buffer_control control);
enum backtrace_buffer_control int_to_control (const int i);
const char * control_to_name (const enum backtrace_buffer_control control);
void print_control (enum backtrace_buffer_control control, const char * prefix, const char * suffix);
void print_op_code(l4_uint64_t op_code, const char * prefix, const char * suffix);

const char * backtrace_buffer_control_names [backtrace_buffer_control_count] = {
	"START",
	"STOP",
	"RESET",
	"SET_TIMESTEP",
	"GET_TIMESTEP",
	"IS_RUNNING",
	"WRITE_STATS",
	"NO_ENTRY",
};

int control_to_int (const enum backtrace_buffer_control control) {
	for (int i = 0; i < backtrace_buffer_control_count; i++)
		if ((1 << i) & control)
			return i;
	return -1;
}
enum backtrace_buffer_control int_to_control (const int i) {
	return (enum backtrace_buffer_control) (1 << i);
}
const char * control_to_name (const enum backtrace_buffer_control control) {
	int i = control_to_int(control);
	if (i == -1)
		return NULL;
	return backtrace_buffer_control_names[i];
}
void print_control (enum backtrace_buffer_control control, const char * prefix, const char * suffix) {
	if (!ubt_debug)
		return;

	printf("%s", prefix);
	int i = control_to_int(control);
	bool first = true;
	while (i != -1) {
		if (first)
			first = false;
		else
			printf(" | ");

		printf("%s", backtrace_buffer_control_names[i]);
		control = (enum backtrace_buffer_control) (control & ~(int_to_control(i)));
		i = control_to_int(control);
	}
	if (first)
		printf("NONE");
	printf("%s", suffix);
}
void print_op_code(l4_uint64_t op_code, const char * prefix, const char * suffix) {
	printf("%s", prefix);
	switch (op_code) {
	case L4_DEBUGGER_GET_BTB_SECTION: printf("L4_DEBUGGER_GET_BTB_SECTION"); break;
	case L4_DEBUGGER_BTB_CONTROL:     printf("L4_DEBUGGER_BTB_CONTROL");     break;
	default:                          printf("<OTHER>");                     break;
	}
	printf("%s", suffix);
}

enum backtrace_buffer_protocol {
	FULL_SECTION_ONLY = 1,
};

static inline
void print_utcb(
	const char * prefix,
	l4_utcb_t * utcb,
	l4_msgtag_t tag,
	bool print_enums
) {
	if (!ubt_debug)
		return;

	unsigned long words = l4_msgtag_words(tag);
	printf(
		"%s l4_msgtag (%ld, %d, %d, %d) ",
		prefix,
		l4_msgtag_label(tag),
		l4_msgtag_words(tag),
		l4_msgtag_items(tag),
		l4_msgtag_flags(tag)
	);

	printf("%s [", prefix);
	l4_umword_t * values = l4_utcb_mr_u(utcb)->mr;
	for (unsigned i = 0; i < words; i++) {
		printf("%d: %6ld", i, values[i]);

		if (print_enums) {
			if (i == 0)
				print_op_code(values[i], " (", ")");
			if (i == 1 && values[0] == L4_DEBUGGER_BTB_CONTROL)
				print_control((enum backtrace_buffer_control) values[i], " (", ")");
		}

		if (i < words - 1)
			printf(", ");
	}
	printf("]\n");
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

	print_utcb("=>>", utcb, tag, true);

	l4_msgtag_t syscall_result = l4_invoke_debugger(cap, tag, utcb);

	print_utcb("<<=", utcb, syscall_result, false);

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

	print_utcb("=>>", utcb, tag, true);

	l4_msgtag_t syscall_result = l4_invoke_debugger(cap, tag, utcb);

	print_utcb("<<=", utcb, syscall_result, false);

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
l4_debugger_backtracing_set_timestep(l4_cap_idx_t cap, l4_uint64_t trace_interval_us) L4_NOTHROW {
	return l4_debugger_backtracing_control_2(cap, BTB_CONTROL_SET_TIMESTEP, trace_interval_us);
}

static inline l4_msgtag_t
l4_debugger_backtracing_get_timestep(l4_cap_idx_t cap, l4_uint64_t * trace_interval_us) L4_NOTHROW {
	l4_msgtag_t syscall_result = l4_debugger_backtracing_control(cap, BTB_CONTROL_GET_TIMESTEP);

	// assumes tick is 1 ms
	*trace_interval_us = l4_utcb_mr()->mr[1] * 1000;
	return syscall_result;
}

static inline l4_msgtag_t
l4_debugger_backtracing_is_running(l4_cap_idx_t cap, bool * is_running) L4_NOTHROW {
	l4_msgtag_t syscall_result = l4_debugger_backtracing_control(cap, BTB_CONTROL_IS_RUNNING);
	*is_running = l4_utcb_mr()->mr[1];
	return syscall_result;
}

static inline l4_msgtag_t
l4_debugger_backtracing_get_btb_words(l4_cap_idx_t cap, l4_uint64_t * btb_words_ptr) L4_NOTHROW {
	l4_msgtag_t syscall_result = l4_debugger_backtracing_control(cap, BTB_CONTROL_IS_RUNNING);
	*btb_words_ptr = l4_utcb_mr()->mr[0]; // this is always returned unrequested.
	return syscall_result;
}

static inline l4_msgtag_t
l4_debugger_backtracing_write_stats(l4_cap_idx_t cap) L4_NOTHROW {
	return l4_debugger_backtracing_control(cap, BTB_CONTROL_WRITE_STATS);
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

	print_utcb("=*>", utcb, tag, true);

	l4_msgtag_t syscall_result = l4_invoke_debugger(cap, tag, utcb);

	print_utcb("<*=", utcb, syscall_result, false);

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

