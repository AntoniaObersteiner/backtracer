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
#include <unistd.h>

#include <l4/backtracer/block.h>
#include <l4/backtracer/measure.h>

#include "src/btb_export.h"

l4_uint64_t others_control_tracing () {
	l4_uint64_t us_start = l4_tsc_to_us(l4_rdtsc());

	bool is_running = false;

	while (true) {
		l4_debugger_backtracing_is_running(&is_running);
		if (!is_running)
			break;

		l4_usleep(100000); // 100 ms
	}

	return us_start;
}

l4_uint64_t we_control_tracing () {
	l4_uint64_t us_sleep_before_tracing = 100000;
	l4_uint64_t us_trace_interval = 100000;
	l4_uint64_t us_stop = measure_start(us_sleep_before_tracing, us_trace_interval);

	// how long to let tracing happen before stopping and exporting.
	sleep(1);

	return us_start;
}

void try_to_shutdown () {
	l4_cap_idx_t pfc_cap = l4re_env_get_cap("pfc");
	bool is_valid = l4_is_valid_cap(pfc_cap) > 0;
	printf(">>> pfc_cap %ld is %svalid <<<\n", pfc_cap, is_valid ? "" : "not ");

	printf(
		"====================\n"
		"== shutting down! ==\n"
		"====================\n"
	);
	// TODO: this does not yet work. I don't know why.
	l4_platform_ctl_system_shutdown(pfc_cap, 0);
}

int main(void) {
	l4_uint64_t us_init = measure_init();

	l4_uint64_t us_start = true ? others_control_tracing() : we_control_tracing();
	l4_uint64_t us_stop  = measure_stop();

	unsigned long remaining_words;
	do {
		remaining_words = export_backtrace_buffer_section(dbg_cap, false, true);
	} while (remaining_words);

	measure_print(us_init, us_start, us_stop);

	try_to_shutdown();

	printf("everything exported, press Ctrl-A and then X\n");
}
