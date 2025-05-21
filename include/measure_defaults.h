#pragma once
// this file can be written by measure.py to automate overhead measurements
static const l4_uint64_t us_sleep_before_tracing = 120000;
// in case of !app_controls_tracing, uses first. unit: 1  m  us
static const l4_uint64_t us_trace_intervals [] =  {     500000 };
// above numbers might be skewed by uninterruptible syscalls, where the tick counter does not ++
static const l4_uint64_t trace_interval_count = 1;
static const l4_uint64_t measure_rounds = 1;
static const int do_overhead = 0;
// for backtracer/main.cc
static const int do_export = 1;
static const int app_controls_tracing = 0;
static const int app_prints_steps = 0;

// used when app_controls_tracing == 0.
// the backtracer traces for this duration and then starts printing.
//                                              s  m  u
static const int us_backtracer_waits_for_app = 20000000;

// used when app_controls_tracing = 1. how many rounds to wait for app to start.
// useful when backtracer is scheduled after app and waiting for app to start backtracing is futile.
static const int rounds_backtracer_waits_for_start = 20;

// syscall debugging infos, no backtracer debugging infos
static const int ubt_debug = 1;
