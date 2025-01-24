
#include <l4/sys/debugger.h>
#include <l4/re/env.h>
#include <l4/sys/irq.h>
#include <l4/sys/platform_control.h>
#include <l4/sys/linkage.h>
#include <l4/sys/types.h>
#include <l4/util/rdtsc.h>

l4_uint64_t measure_init () {
	l4_cpu_time_t tsc_init = l4_rdtsc ();
	l4_calibrate_tsc(l4re_kip());
	l4_uint64_t us_init  = l4_tsc_to_us (tsc_init);

	l4_cap_idx_t dbg_cap = L4_BASE_DEBUGGER_CAP;
	bool is_valid = l4_is_valid_cap(dbg_cap) > 0;
	printf(">>> dbg_cap %ld is %svalid <<<\n", dbg_cap, is_valid ? "" : "not ");

	return us_init;
}

l4_uint64_t measure_start (l4_uint64_t wait_us, l4_uint64_t trace_interval_us) {
	printf("wait for all to settle\n");
	l4_usleep(wait_us);

	l4_debugger_backtracing_set_timestep(dbg_cap, trace_interval);
	l4_debugger_backtracing_start(dbg_cap);
	l4_cpu_time_t tsc_start = l4_rdtsc ();

	l4_uint64_t us_start = l4_tsc_to_us (tsc_start);

	return us_start;
}

l4_uint64_t measure_stop () {
	l4_debugger_backtracing_stop(dbg_cap);
	l4_cpu_time_t tsc_stop = l4_rdtsc ();

	l4_uint64_t us_stop  = l4_tsc_to_us (tsc_stop);

	return us_stop;
}

void measure_print (l4_uint64_t us_init, l4_uint64_t us_start, l4_uint64_t us_stop) {
	// we print the start and stop time of the backtracer so we
	// know that the app did its time measurment in between and was affected.
	printf("abs time main %16llx us\n", us_main);
	printf("=?=?= [start] %16llx us\n", us_start);
	printf("=?=?= [stop]  %16llx us\n", us_stop);
	printf("start to stop %16llx us\n", us_stop  - us_start);

	printf("      [start] %16.3f s\n", (double) (us_start - us_main)  / 1000000.0);
	printf("      [stop]  %16.3f s\n", (double) (us_stop  - us_main)  / 1000000.0);
	printf("start to stop %16.3f s\n", (double) (us_stop  - us_start) / 1000000.0);
}
