
// this file is written by measure.py to automate overhead measurements
static const l4_uint64_t ms_to_us = 1000;
static const l4_uint64_t us_sleep_before_tracing = 120 * ms_to_us;
static const l4_uint64_t us_trace_interval = 80 * ms_to_us;
static const int do_overhead = 0;
// for backtracer/main.cc
static const int do_export = 1;
static const int app_controls_tracing = 1;
static const int app_prints_steps = 0;
