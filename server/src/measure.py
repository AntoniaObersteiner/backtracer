#!/usr/bin/python3

import argparse
import os
import re
import pandas as pd
import seaborn as sns
from matplotlib import pyplot as plt


argparser = argparse.ArgumentParser(
    prog = "measure.py",
    description = "controls overhead measurements "
        "and flamegraph generation for multiple applications.",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter,
)
argparser.add_argument(
    "--measure-rounds",
    type = int,
    default = 10,
    help = "how many round of measurements to do",
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
    "--plot",
    action = "store_const",
    const = True,
    default = False,
    help = "generate a plot of the durations of the programs",
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
    default = [.001, .002, .005, .010, .020, .040, .080],
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
    s_trace_intervals = [0.001, 0.002, 0.005, 0.010,],
    measure_rounds = 10,
    do_overhead = True,
    do_export = False,
):
    us_sleep_before_tracing = int(1000_000 * s_sleep_before_tracing)
    trace_interval_count = len(s_trace_intervals)
    us_trace_intervals      = "{" + ", ".join(
        str(int(1000_000 * s_trace_interval))
        for s_trace_interval in s_trace_intervals
    ) + "}"

    # we don't necessarily have stdbool, so use 0 and 1
    c_do_overhead = 1 if do_overhead else 0;
    c_do_export   = 1 if do_export   else 0;

    text = f"""
// this file is written by measure.py to automate overhead measurements
static const l4_uint64_t us_sleep_before_tracing = {us_sleep_before_tracing};
static const l4_uint64_t trace_interval_count = {trace_interval_count};
static const l4_uint64_t us_trace_intervals [] = {us_trace_intervals};
static const l4_uint64_t measure_rounds = {measure_rounds};
static const int do_overhead = {c_do_overhead};
// for backtracer/main.cc
static const int do_export = {c_do_export};
static const int app_controls_tracing = 1;
static const int app_prints_steps = 0;
// no syscall debugging infos, no backtracer debugging infos
static const int quiet = 1;
"""
    with open(measure_defaults, "w") as file:
        file.write(text)

def make_external(*args):
    command = "make -f External.make " + " ".join(args)
    print(command)
    return_code = os.system(command)
    if return_code != 0:
        raise OSError(return_code, command)

def measure_overhead(app, args):
    label = f"measure_loop"
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

    csv_filename = f"data/{label}/{app}.csv"
    write_measurement_csv(
        f"data/{label}/{app}.cleaned",
        csv_filename,
    )

    return csv_filename

# this is how the measure_print() function of measure.h formats its output
start_stop_regex = re.compile(
    r".*=\?=\?= +\[(start|stop)] +"
    r"\((.*)\) +(0x)?([a-fA-F0-9]+) +us.*"
)
export_regex = re.compile(
    r".*<\*= +\[0: ([0-9]+).*].*"
)
measure_line_regex = re.compile(
    r".*=\.=\.= +\[(.+)\] +s"
)
def write_measurement_csv(input_filename, output_filename):
    result = {}

    with open(output_filename, "w") as output_file:
        with open(input_filename, "r") as input_file:
            for line in input_file:
                m = measure_line_regex.match(line)
                if m:
                    print(f"matched line {line!r}")

                    output_file.write(m[1] + "\n")

def savefig (
    filename,
):
    print(f"saving to {filename!r}")
    plt.savefig(filename)
    # clear figure for next plot
    plt.clf()

def plot_app_durations(data):
    data = data.query("program != 'backtracer' and program != 'bt-export'")

    data["trace_interval"] = data["trace_interval"].astype(str) # to avoid legend bins

    sns.barplot(
        data = data,
        x = "program",
        y = "duration",
        hue = "trace_interval",
    )
    print(data)
    savefig("data/app_durations.svg")

def plot_btb_words(data):
    data["trace_interval"] = data["trace_interval"].astype(str) # to avoid legend bins

    sns.barplot(
        data = data,
        x = "program",
        y = "btb_words",
        #y = "btb [u64 words]",
        hue = "trace_interval",
    )
    savefig("data/btb_words.svg")

def main():
    args = argparser.parse_args()

    if args.test:
        test()
        return

    write_measure_defaults(
        s_sleep_before_tracing = args.sleep_before_tracing,
        s_trace_intervals = args.trace_intervals,
        measure_rounds = args.measure_rounds,
        do_overhead = args.overhead,
        do_export = args.flamegraph,
    )
    measurements = pd.concat(
        pd.read_csv(app_csv_filename := measure_overhead(app, args))
        for app in args.apps
    )

    csv_filename = "data/measurements.csv"
    with open(csv_filename, "w") as csv_file:
        measurements.to_csv(csv_file)

    if args.plot:
        plot_app_durations(measurements)
        plot_btb_words(measurements)


if __name__ == "__main__":
    main()
