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
#include <l4/sys/platform_control.h>
#include <l4/sys/linkage.h>
#include <l4/sys/types.h>
#include <unistd.h>

#include "src/block.h"
#include "src/btb_syscall.h"

int main(void) {
	l4_cpu_time_t time_main = l4_rdtsc ();

	l4_cap_idx_t dbg_cap = L4_BASE_DEBUGGER_CAP;
	bool is_valid = l4_is_valid_cap(dbg_cap) > 0;
	printf(">>> dbg_cap %ld is %svalid <<<\n", dbg_cap, is_valid ? "" : "not ");

	l4_cap_idx_t pfc_cap = l4re_env_get_cap("pfc");
	is_valid = l4_is_valid_cap(pfc_cap) > 0;
	printf(">>> pfc_cap %ld is %svalid <<<\n", pfc_cap, is_valid ? "" : "not ");

	printf("wait a second...");
	sleep(1);

	l4_debugger_backtracing_set_timestep(dbg_cap, 10);
	l4_debugger_backtracing_start(dbg_cap);
	l4_cpu_time_t time_start = l4_rdtsc ();

	printf("trace for some time...\n");
	sleep(1);
	l4_debugger_backtracing_stop(dbg_cap);
	l4_cpu_time_t time_stop = l4_rdtsc ();

	unsigned long remaining_words;
	do {
		remaining_words = export_backtrace_buffer_section(dbg_cap, false, true);
	} while (remaining_words);

	l4_uint64_t us_main  = l4_tsc_to_us (time_main);
	l4_uint64_t us_start = l4_tsc_to_us (time_start);
	l4_uint64_t us_stop  = l4_tsc_to_us (time_stop);

	printf("abs time main %8lx us\n", us_main);
	printf("=?=?= [start] %8lx us\n", us_start - us_main);
	printf("=?=?= [stop]  %8lx us\n", us_stop  - us_main);
	printf("start to stop %8lx us\n", us_stop  - us_start);

	printf(
		"====================\n"
		"== shutting down! ==\n"
		"====================\n"
	);
	l4_platform_ctl_system_shutdown(pfc_cap, 0);

	printf("everything exported, press Ctrl-A and then X\n");
}
