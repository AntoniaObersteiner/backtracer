
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
void measure_print (
	const char * program_name,
	l4_uint64_t us_init,
	l4_uint64_t us_start,
	l4_uint64_t us_stop
);
static int measure_loop(
	int (*workload) (void *, l4_uint64_t),
	void * workload_arg,
	l4_uint64_t workload_rounds,
	l4_uint64_t us_init,
	const char * program_name
);

static const l4_cap_idx_t dbg_cap = L4_BASE_DEBUGGER_CAP;

inline l4_uint64_t measure_init (void) {
	l4_cpu_time_t tsc_init = l4_rdtsc ();
	l4_calibrate_tsc(l4re_kip());
	l4_uint64_t us_init  = l4_tsc_to_us (tsc_init);

	bool is_valid = l4_is_valid_cap(dbg_cap) > 0;
	printf(">>> dbg_cap %ld is %svalid <<<\n", dbg_cap, is_valid ? "" : "not ");

	return us_init;
}

inline bool backtracing_is_running (void) {
	bool is_running = false;
	l4_debugger_backtracing_is_running(dbg_cap, &is_running);
	return is_running;
}

inline l4_uint64_t measure_start (l4_uint64_t wait_us, l4_uint64_t trace_interval_us) {
	printf("wait for all to settle\n");
	l4_usleep(wait_us);

	l4_debugger_backtracing_set_timestep(dbg_cap, trace_interval_us);
	l4_debugger_backtracing_start(dbg_cap);

	l4_cpu_time_t tsc_start = l4_rdtsc ();
	l4_uint64_t us_start = l4_tsc_to_us (tsc_start);

	return us_start;
}

inline l4_uint64_t measure_stop (void) {
	l4_debugger_backtracing_stop(dbg_cap);
	l4_cpu_time_t tsc_stop = l4_rdtsc ();

	l4_uint64_t us_stop  = l4_tsc_to_us (tsc_stop);

	bool is_running;
	l4_debugger_backtracing_is_running(dbg_cap, &is_running);
	if (is_running) printf(
		"after stoppping: kernel backtracer is %srunning!\n",
		is_running ? "still " : "not "
	);

	return us_stop;
}

static inline l4_uint64_t measure_btb_words (void) {
	l4_uint64_t btb_words;

	l4_debugger_backtracing_get_btb_words(dbg_cap, &btb_words);

	return btb_words;
}

inline void measure_print (
	const char * program_name,
	l4_uint64_t us_init,
	l4_uint64_t us_start,
	l4_uint64_t us_stop
) {
	// we print the start and stop time of the backtracer so we
	// know that the app did its time measurement in between and was affected.
	printf("abs time init %16llx us\n", us_init);
	printf("=?=?= [start] (%s) %16llx us\n", program_name, us_start);
	printf("=?=?= [stop]  (%s) %16llx us\n", program_name, us_stop);
	printf("start to stop %16llx us\n", us_stop  - us_start);

	printf("      [start] (%s) %16.3f s\n", program_name, (double) (us_start - us_init)  / 1000000.0);
	printf("      [stop]  (%s) %16.3f s\n", program_name, (double) (us_stop  - us_init)  / 1000000.0);
	printf("start to stop %16.3f s\n", (double) (us_stop  - us_start) / 1000000.0);
}

enum measure_format {
	long_int_us,
	double_s,
};

const char * const csv_header = "=.=.= [measure_round,trace_interval,init,start,stop,duration,btb_words,main_app,part_app]  s\n",

static inline void print_measure_line (
	const char * main_program_name,
	const char * part_program_name,
	l4_uint64_t us_trace_interval,
	l4_uint64_t measure_round,
	l4_uint64_t us_init,
	l4_uint64_t us_start,
	l4_uint64_t us_stop,
	l4_uint64_t btb_words,
	enum measure_format format
) {
	// we print the start and stop time of the backtracer so we
	// know that the app did its time measurement in between and was affected.
	l4_uint64_t rel_start = us_start - us_init;
	l4_uint64_t rel_stop  = us_stop  - us_init;
	l4_uint64_t duration  = us_stop  - us_start;
	double s_trace_interval = (double)  us_trace_interval  / 1000000;
	double     s_init       = (double)  us_init            / 1000000;
	double s_rel_start      = (double) rel_start           / 1000000;
	double s_rel_stop       = (double) rel_stop            / 1000000;
	double s_duration       = (double) duration            / 1000000;

	switch (format) {
	case long_int_us:
	printf(
		"=!=!= [%3lld,%16lld,%16lld,%16lld,%16lld,%16lld,%16lld,%s,%s] us\n",
		measure_round,
		us_trace_interval,
		us_init,
		rel_start,
		rel_stop,
		duration,
		btb_words,
		main_program_name,
		part_program_name
	);
	break;
	case double_s:
	printf(
		"=.=.= [%3lld,%16.3f,%16.3f,%16.6f,%16.6f,%16.6f,%16lld,%s,%s]  s\n",
		measure_round,
		s_trace_interval,
		s_init,
		s_rel_start,
		s_rel_stop,
		s_duration,
		btb_words,
		main_program_name,
		part_program_name
	);
	break;
	}
}

int measure_loop(
	int (*workload) (void *, l4_uint64_t),
	void * workload_arg,
	l4_uint64_t workload_rounds,
	l4_uint64_t us_init,
	const char * program_name
) {
	l4_uint64_t us_starts [trace_interval_count][measure_rounds];
	l4_uint64_t us_stops  [trace_interval_count][measure_rounds];
	l4_uint64_t btb_words [trace_interval_count][measure_rounds];

	int result = 0; // to avoid optimizing out
	for (l4_uint64_t trace_interval_index = 0; trace_interval_index <  trace_interval_count; trace_interval_index++) {
		l4_uint64_t us_trace_interval = us_trace_intervals[trace_interval_index];
	for (l4_uint64_t        measure_round = 0;        measure_round <        measure_rounds;        measure_round++) {
		us_starts[trace_interval_index][measure_round] = measure_start(us_sleep_before_tracing, us_trace_interval);
	for (l4_uint64_t       workload_round = 0;       workload_round <       workload_rounds;       workload_round++) {
		result += workload(workload_arg, workload_round);
	}
		us_stops [trace_interval_index][measure_round] = measure_stop();
		btb_words[trace_interval_index][measure_round] = measure_btb_words();

		// clear buffer after measuring its content
		l4_debugger_backtracing_reset(dbg_cap);
	}
	}

	for (l4_uint64_t trace_interval_index = 0; trace_interval_index <  trace_interval_count; trace_interval_index++) {
	for (l4_uint64_t        measure_round = 0;        measure_round <        measure_rounds;        measure_round++) {
		print_measure_line (
			main_program_name,
			part_program_name,
			us_trace_intervals[trace_interval_index],
			measure_round,
			us_init,
			us_starts[trace_interval_index][measure_round],
			us_stops [trace_interval_index][measure_round],
			btb_words[trace_interval_index][measure_round],
			long_int_us
		);
	}
	}

	for (l4_uint64_t trace_interval_index = 0; trace_interval_index <  trace_interval_count; trace_interval_index++) {
	for (l4_uint64_t        measure_round = 0;        measure_round <        measure_rounds;        measure_round++) {
		print_measure_line (
			main_program_name,
			part_program_name,
			measure_round,
			us_init,
			us_starts[trace_interval_index][measure_round],
			us_stops [trace_interval_index][measure_round],
			btb_words[trace_interval_index][measure_round],
			double_s
		);
	}
	}

	return result;
}
