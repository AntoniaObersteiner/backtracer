#!/usr/bin/python3

import argparse
import os
import re

argparser = argparse.ArgumentParser(
    prog = "measure.py",
    description = "controls overhead measurements "
        "and flamegraph generation for multiple applications.",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter,
)
argparser.add_argument(
    "--sleep-before-tracing",
    type = float,
    default = .120,
    help = "initial wait of the apps in seconds "
        "so the backtracer has time to start.",
)
argparser.add_argument(
    "--test",
    action = "store_const",
    const = True,
    default = False,
    help = "run the unit tests of this program and exit.",
)
argparser.add_argument(
    "--target",
    choices = ["qemu", "erwin"],
    default = "qemu",
    help = "where to deploy. install to 'erwin' -> pxe-boot by hand.",
)
argparser.add_argument(
    "--trace-intervals",
    nargs = "+",
    metavar = "Interval",
    type = float,
    default = [.005, .010, .020, .040, .080],
    help = "tracing intervals in seconds.",
)
argparser.add_argument(
    "--apps",
    nargs = "+",
    metavar = "App",
    type = str,
    default = ["qsort", "hello", "stress_malloc", "stress_rng", "stress_vfs"],
    help = "what programs to backtrace",
)
argparser.add_argument(
    "--overhead",
    action = "store_const",
    const = True,
    default = False,
    help = "start the measurement in overhead mode (no export)",
)
argparser.add_argument(
    "--flamegraph",
    action = "store_const",
    const = True,
    default = False,
    help = "don't just measure overhead, also export data and plot",
)

package_root = os.path.join("..", "..")
measure_defaults = os.path.join(package_root, "include", "measure_defaults.h")

s_from_us = .000_001

def write_measure_defaults(
    s_sleep_before_tracing = 0.120,
    s_trace_interval = 0.010,
    do_overhead = True,
    do_export = False,
):
    ms_sleep_before_tracing = int(1000 * s_sleep_before_tracing)
    ms_trace_interval       = int(1000 * s_trace_interval)

    # we don't necessarily have stdbool, so use 0 and 1
    c_do_overhead = 1 if do_overhead else 0;
    c_do_export   = 1 if do_export   else 0;

    text = f"""
// this file is written by measure.py to automate overhead measurements
static const l4_uint64_t ms_to_us = 1000;
static const l4_uint64_t us_sleep_before_tracing = {ms_sleep_before_tracing} * ms_to_us;
static const l4_uint64_t us_trace_interval = {ms_trace_interval} * ms_to_us;
static const int do_overhead = {c_do_overhead};
// for backtracer/main.cc
static const int do_export = {c_do_export};
static const int app_controls_tracing = 1;
"""
    with open(measure_defaults, "w") as file:
        file.write(text)

def make_external(*args):
    command = "make -f External.make " + " ".join(args)
    print(command)
    return_code = os.system(command)
    if return_code != 0:
        raise OSError(return_code, command)

def measure_overhead(app, us_trace_interval, args):
    label = f"measure{us_trace_interval}us"
    if args.target == "erwin":
        raise NotImplementedError(
            f"using target 'erwin' (i.e. pxe-boot) "
            f"is not yet implemented!"
        )

    if args.flamegraph:
        make_external(
            f"LABEL={label}",
            f"data/{label}/{app}.log",
        )
    else:
        make_external(
            f"LABEL={label}",
            f"data/{label}/{app}.cleaned",
        )


    timespans = read_timespans_per_app(f"data/{label}/{app}.cleaned")
    return timespans

# this is how the measure_print() function of measure.h formats its output
start_stop_regex = re.compile(
    r".*=\?=\?= +\[(start|stop)] +"
    r"\((.*)\) +(0x)?([a-fA-F0-9]+) +us.*"
)
def read_timespans_per_app(filename):
    # Dict[app: str -> Dict[
    #    start_or_stop: str -> time_value_in_seconds: float
    # ]]
    result = {}

    with open(filename, "r") as file:
        for line in file:
            m = start_stop_regex.match(line)
            if m:
                print(f"matched line {line!r}")

                app = m[2]
                start_or_stop = m[1]
                if app not in result:
                    result[app] = {}
                if start_or_stop in result[app]:
                    raise KeyError(
                        f"{start_or_stop!r} already exists for {app!r}!"
                    )

                us_time_value = int(m[4], 16)
                result[app][start_or_stop] = us_time_value * s_from_us

    return result

def test_read_timespans():
    filename = ".test.cleaned"
    t1 =    0.123_456
    t2 = 1231.231_231
    t3 =    0.000_012
    t4 =    0.999_999
    h1 = hex(int(t1 * 1000_000))
    h2 = hex(int(t2 * 1000_000))[2:]
    h3 = hex(int(t3 * 1000_000))[2:].upper() # 0XC is not hex
    h4 = hex(int(t4 * 1000_000)).lower()
    with open(filename, "w") as file:
        file.write("=?=?= [start] (test) " + h1 + " us\n")
        file.write("=?=?= [stop] (test)    " + h2 + " us\n")
        file.write("=?=?=  [start]  (test-2) " + h3 + "    us\n")
        file.write("qsort | =?=?=   [stop] (test-2) " + h4 + " us\n")

    timespans = read_timespans_per_app(filename)
    assert "test" in timespans
    assert "test-2" in timespans
    assert "start" in timespans["test"]
    assert "stop"  in timespans["test"]
    assert "start" in timespans["test-2"]
    assert "stop"  in timespans["test-2"]
    assert timespans["test"  ]["start"] == t1
    assert timespans["test"  ]["stop"]  == t2
    assert timespans["test-2"]["start"] == t3
    assert timespans["test-2"]["stop"]  == t4

    os.remove(filename)

    print("passed test of read_timespans_per_app")

def test():
    test_read_timespans()

def main():
    args = argparser.parse_args()

    if args.test:
        test()
        return

    for trace_interval in args.trace_intervals:
        write_measure_defaults(
            s_sleep_before_tracing = args.sleep_before_tracing,
            s_trace_interval = trace_interval,
            do_overhead = args.overhead,
            do_export = args.flamegraph,
        )
        for app in args.apps:
            timespans = measure_overhead(
                app,
                int(1000_000 * trace_interval),
                args
            )
            print(timespans)

if __name__ == "__main__":
    main()
