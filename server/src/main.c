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
#include <stdbool.h>
#include <sys/mman.h>
#include <l4/sys/debugger.h>
#include <l4/re/env.h>
#include <l4/sys/irq.h>
#include <unistd.h>

#include "block.h"
#include "btb_syscall.h"

int main(void) {
	l4_cap_idx_t dbg_cap = L4_BASE_DEBUGGER_CAP;
	bool is_valid = l4_is_valid_cap(dbg_cap) > 0;
	printf(">>> dbg_cap %ld is %svalid <<<\n", dbg_cap, is_valid ? "" : "not ");

	printf("wait a second...");
	sleep(1);
	l4_debugger_backtracing_start(dbg_cap);

	printf("trace for 4 seconds...");
	sleep(4);
	l4_debugger_backtracing_stop(dbg_cap);

	unsigned long remaining_words;
	do {
		remaining_words = export_backtrace_buffer_section(dbg_cap, false);
	} while (remaining_words);

	printf("everything exported, press Ctrl-A and then X\n");
}
