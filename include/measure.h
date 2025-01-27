
#include <stdbool.h>
#include <l4/sys/debugger.h>
#include <l4/sys/debugger.h>
#include <l4/re/env.h>
#include <l4/sys/irq.h>
#include <l4/sys/platform_control.h>
#include <l4/sys/linkage.h>
#include <l4/sys/types.h>
#include <l4/util/rdtsc.h>
#include <l4/util/util.h>
#include <l4/backtracer/btb_control.h>

l4_uint64_t measure_init (void);
l4_uint64_t measure_start (l4_uint64_t wait_us, l4_uint64_t trace_interval_us);
l4_uint64_t measure_stop (void);
void measure_print (l4_uint64_t us_init, l4_uint64_t us_start, l4_uint64_t us_stop);

static const l4_cap_idx_t dbg_cap = L4_BASE_DEBUGGER_CAP;

inline l4_uint64_t measure_init (void) {
	l4_cpu_time_t tsc_init = l4_rdtsc ();
	l4_calibrate_tsc(l4re_kip());
	l4_uint64_t us_init  = l4_tsc_to_us (tsc_init);

	bool is_valid = l4_is_valid_cap(dbg_cap) > 0;
	printf(">>> dbg_cap %ld is %svalid <<<\n", dbg_cap, is_valid ? "" : "not ");

	return us_init;
}

inline l4_uint64_t measure_start (l4_uint64_t wait_us, l4_uint64_t trace_interval_us) {
	printf("wait for all to settle\n");
	l4_usleep(wait_us);

	l4_debugger_backtracing_set_timestep(dbg_cap, trace_interval_ticks);
	l4_debugger_backtracing_start(dbg_cap);
	l4_cpu_time_t tsc_start = l4_rdtsc ();

	l4_uint64_t us_start = l4_tsc_to_us (tsc_start);

	return us_start;
}

inline l4_uint64_t measure_stop (void) {
	l4_debugger_backtracing_stop(dbg_cap);
	l4_cpu_time_t tsc_stop = l4_rdtsc ();

	l4_uint64_t us_stop  = l4_tsc_to_us (tsc_stop);

	return us_stop;
}

inline void measure_print (l4_uint64_t us_init, l4_uint64_t us_start, l4_uint64_t us_stop) {
	// we print the start and stop time of the backtracer so we
	// know that the app did its time measurment in between and was affected.
	printf("abs time init %16llx us\n", us_init);
	printf("=?=?= [start] %16llx us\n", us_start);
	printf("=?=?= [stop]  %16llx us\n", us_stop);
	printf("start to stop %16llx us\n", us_stop  - us_start);

	printf("      [start] %16.3f s\n", (double) (us_start - us_init)  / 1000000.0);
	printf("      [stop]  %16.3f s\n", (double) (us_stop  - us_init)  / 1000000.0);
	printf("start to stop %16.3f s\n", (double) (us_stop  - us_start) / 1000000.0);
}
