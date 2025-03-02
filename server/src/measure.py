#!/usr/bin/python3

import os
import re
import argparse

class MyFormatter(
    argparse.ArgumentDefaultsHelpFormatter,
    argparse.RawDescriptionHelpFormatter,
): pass

argparser = argparse.ArgumentParser(
    prog = "measure.py",
    description = "controls overhead measurements "
        "and flamegraph generation for multiple applications.",
    formatter_class = MyFormatter,
    epilog = """
EXTRA KCONFIG
Separate Kconfig files are swapped in in the build directory of fiasco and the
build scripts take care to recompile. This allows to gather runtimes for
different configs in separate directories (data/{label}/). This is done since
the files are just named {app}.csv.
The config files must be created beforehand and be present in the fiasco build
dir, see External.make::fiasco_config and ::fiasco_swap_kconfig.
Format: {filename}:{trace_intervals}:{plot_label}:[label]
- filename must not contain weird characters like ",:; \\n".
  both {filename}.out and {filename}.h are copied onto globalconfig.{out,h}.
- trace intervals is comma-separated.
- plot_label should probably not contain ":", but who knows.
- label is optional (the : before is not!) and defaults to kconfig_filename.
  it's the --label, so a directory in data/
"""
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
    default = [.001, .010, 0],
    help = "tracing intervals in seconds. write 99 or similar to distiguish nojdb runs, see there.",
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
    "--export",
    action = "store_const",
    const = True,
    default = False,
    help = "don't just measure overhead, also export data and plot",
)
argparser.add_argument(
    "--app-prints-steps",
    action = "store_const",
    const = True,
    default = False,
    help = "print number of workload round, incurs printing overhead",
)
argparser.add_argument(
    "--ubt-debug",
    action = "store_const",
    const = True,
    default = False,
    help = "print syscall logs and other debug in user backtracer",
)
argparser.add_argument(
    "--label",
    default = "measure_loop",
    help = "which subdir of data/ to use",
)
class ExtraKconfig:
    def __init__(self, filename, trace_intervals, plot_label, label = None):
        self.filename = filename
        self.trace_intervals = trace_intervals
        self.plot_label = plot_label
        if label is None:
            label = self.filename
        self.label = label

    def __str__(self):
        return (
            f"{self.filename}:"
            f"{','.join(self.trace_intervals)}:"
            f"{self.plot_label}"
        )

    def __repr__(self):
        return (
            f"{self.__class__.__name__}("
            f"{self.filename = }, "
            f"{self.trace_intervals = }, "
            f"{self.plot_label = }, "
            f"{self.label = })"
        )

    extra_kconfig_format = re.compile(r"(.+):([0-9]+):(.+):(.*)")
    def parse(self, text):
        m = extra_kconfig_format.fullmatch(text)
        return ExtraKconfig(
            m[1],
            list(float(v for v in m[2].split(","))),
            m[3],
            m[4] if m[4] else None
        )

    def swap_in(self):
        make_external(
            f"KCONFIG={self.filename}",
            f"fiasco_swap_kconfig",
        )

    def modified_args(self, args):
        new_args = deepcopy(args)
        new_args.trace_intervals = self.trace_intervals
        new_args.label = self.label
        return new_args

    def default_kconfig(args):
        return ExtraKconfig(
            filename = "fullkbt",
            trace_intervals = args.trace_intervals,
            plot_label = None, # actual trace interval
            label = args.label,
        )

argparser.add_argument(
    "--default-kconfig-name",
    default = "fullkbt",
    help = "the name of the default kconfig.",
)
argparser.add_argument(
    "--extra-kconfigs",
    nargs = "+",
    type = ExtraKconfig.parse,
    default = [
        ExtraKconfig("nojdb", [99], "kein JDB"),
        ExtraKconfig("nokbt", [98], "kein KBT"),
    ],
    help = "create / load special data for runs without jdb. see EXTRA KCONFIG below.",
)
argparser.add_argument(
    "--no-extra-kconfigs",
    action = "store_const",
    const = [],
    dest = "extra_kconfigs",
    help = "don't create or load special data for runs without jdb/kbt/....",
)

package_root = os.path.join("..", "..")
measure_defaults = os.path.join(package_root, "include", "measure_defaults.h")

import pandas as pd
import seaborn as sns
from matplotlib import pyplot as plt
from copy import deepcopy

s_from_us = .000_001

def write_measure_defaults(args):
    us_sleep_before_tracing = int(1000_000 * args.sleep_before_tracing)
    trace_interval_count = len(args.trace_intervals)
    us_trace_intervals      = "{" + ", ".join(
        str(int(1000_000 * s_trace_interval))
        for s_trace_interval in args.trace_intervals
    ) + "}"

    # we don't necessarily have stdbool, so use 0 and 1
    c_do_overhead      = 1 if args.overhead         else 0;
    c_do_export        = 1 if args.export           else 0;
    c_app_prints_steps = 1 if args.app_prints_steps else 0;
    c_ubt_debug        = 1 if args.ubt_debug        else 0;

    text = f"""
// this file is written by measure.py to automate overhead measurements
static const l4_uint64_t us_sleep_before_tracing = {us_sleep_before_tracing};
static const l4_uint64_t trace_interval_count = {trace_interval_count};
static const l4_uint64_t us_trace_intervals [] = {us_trace_intervals};
static const l4_uint64_t measure_rounds = {args.measure_rounds};
static const int do_overhead = {c_do_overhead};
// for backtracer/main.cc
static const int do_export = {c_do_export};
static const int app_controls_tracing = 1;
static const int app_prints_steps = {c_app_prints_steps};
// no syscall debugging infos, no backtracer debugging infos
static const int ubt_debug = {c_ubt_debug};
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
    label = args.label
    if args.target == "erwin":
        raise NotImplementedError(
            f"using target 'erwin' (i.e. pxe-boot) "
            f"is not yet implemented!"
        )

    if args.export:
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

def plot_app_durations(
    data,
    args,
    trace_interval_selection = {99, 0, .001, .01},
):
    data = data.query("program != 'backtracer' and program != 'bt-export'")
    data = data.query(" or ".join(
        f"trace_interval == {selection}"
        for selection in trace_interval_selection
    ))

    data["ms_trace_interval"] = data["trace_interval"] * 1000
    # to avoid legend bins
    data["ms_trace_interval"] = data["ms_trace_interval"].astype(int).astype(str)
    for kconfig in args.extra_kconfigs:
        if len(kconfig.trace_intervals) != 1:
            print(f"Warning! there are {len(kconfig.trace_intervals) = } in {kconfig = }!")
        for kconfig_interval in kconfig.trace_intervals:
            data["ms_trace_interval"].replace(
                to_replace = str(int(kconfig_interval * 1000)),
                value = kconfig.plot_label,
                inplace = True,
            )
    data["ms_trace_interval"].replace(
        to_replace = "0",
        value = "nicht aktiviert",
        inplace = True,
    )

    plot = sns.barplot(
        data = data,
        x = "program",
        y = "duration",
        hue = "ms_trace_interval",
    )
    legend = plot.legend()
    legend.set_title("Trace Interval [ms]")
    plot.set_ylabel("Ausführungsdauer [s]")
    plot.set_xlabel("Programm")
    print(data)
    savefig(f"data/{args.label}/app_durations.svg")

def plot_btb_words(data, args):
    data["trace_interval"] = data["trace_interval"].astype(str) # to avoid legend bins

    sns.barplot(
        data = data,
        x = "program",
        y = "btb_words",
        #y = "btb [u64 words]",
        hue = "trace_interval",
    )
    savefig(f"data/{args.label}/btb_words.svg")

def measure_and_write(args, kconfig = None):
    if kconfig is None:
        kconfig = ExtraKconfig.default_kconfig(args)
    kconfig.swap_in()

    write_measure_defaults(args)
    measurements = pd.concat(
        pd.read_csv(app_csv_filename := measure_overhead(app, args))
        for app in args.apps
    )
    measurements["kconfig"] = pd.Series([kconfig.filename for _ in range(len(measurements.index))])

    csv_filename = f"data/{args.label}/measurements.csv"
    with open(csv_filename, "w") as csv_file:
        measurements.to_csv(csv_file)

    return measurements

def main():
    args = argparser.parse_args()

    if args.test:
        test()
        return

    measurements = measure_and_write(args)

    for kconfig in args.extra_kconfigs:
        modified_args = kconfig.modified_args(args)
        kconfig_measurements = measure_and_write(modified_args, kconfig)

        kconfig_measurements = kconfig_measurements.query(" or ".join(
            f"trace_interval == {i}"
            for i in kconfig.trace_intervals
        ))
        measurements = pd.concat([
            measurements,
            kconfig_measurements,
        ])

    if args.plot:
        plot_app_durations(measurements, args)
        plot_btb_words(measurements, args)


if __name__ == "__main__":
    main()
