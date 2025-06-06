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
#include <l4/backtracer/measure_defaults.h>
#include <l4/backtracer/measure.h>

#include "btb_export.h"

static l4_uint64_t others_control_tracing () {
	l4_uint64_t us_start = l4_tsc_to_us(l4_rdtsc());

	l4_uint64_t btb_words_start = get_btb_words_no_entry();
	l4_uint64_t us_sleeptime = 1 * 1000 * 1000; // 1 second
	for (
		int not_started_count = 0;
		not_started_count <= rounds_backtracer_waits_for_start && get_btb_words_no_entry() - btb_words_start == 0;
		not_started_count ++
	) {
		if (ubt_debug)
			printf("backtracing has not written anything yet, wait...\n");
		l4_usleep(us_sleeptime);
	}

	while (backtracing_is_running()) {
		if (ubt_debug)
			printf("backtracing is still running, wait...\n");
		l4_usleep(us_sleeptime);
	}

	return us_start;
}

static l4_uint64_t we_control_tracing () {
    // from measure_defaults.h
	l4_uint64_t us_start = measure_start(us_sleep_before_tracing, us_trace_intervals[0]);

	// how long to let tracing happen before stopping and exporting.
	usleep(us_backtracer_waits_for_app);

	return us_start;
}

static void try_to_shutdown () {
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
	if (!do_export) {
		return 0;
	}
	l4_uint64_t us_init = measure_init();

	l4_uint64_t us_start = (
		app_controls_tracing
		? others_control_tracing()
		: we_control_tracing()
	);
	l4_uint64_t us_stop  = measure_stop();

	l4_uint64_t us_export_start = l4_tsc_to_us(l4_rdtsc());

	unsigned long remaining_words = 1;
	while (do_export && remaining_words) {
		remaining_words = export_backtrace_buffer_section(
			dbg_cap, false, true
		);
	}

	l4_uint64_t us_export_stop = l4_tsc_to_us(l4_rdtsc());

	measure_print("backtracer", us_init, us_start, us_stop);
	measure_print("bt-export", us_init, us_export_start, us_export_stop);

	try_to_shutdown();

	printf("everything exported, press Ctrl-A and then X\n");
}
