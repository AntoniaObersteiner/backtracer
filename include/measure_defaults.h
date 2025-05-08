#pragma once
// this file can be written by measure.py to automate overhead measurements
static const l4_uint64_t us_sleep_before_tracing = 120000;
static const l4_uint64_t trace_interval_count = 1;
static const l4_uint64_t us_trace_intervals [] = {50};
static const l4_uint64_t measure_rounds = 1;
static const int do_overhead = 0;
// for backtracer/main.cc
static const int do_export = 1;
static const int app_controls_tracing = 0;
static const int app_prints_steps = 0;
// used when app_controls_tracing == 0.
// the backtracer traces for this duration and then starts printing.
static const int us_backtracer_waits_for_app =  5000000;
// syscall debugging infos, no backtracer debugging infos
static const int ubt_debug = 0;
