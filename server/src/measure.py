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
static const int app_prints_steps = 0;
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
export_regex = re.compile(
    r".*<\*= +\[0: ([0-9]+).*].*"
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
                result[app][start_or_stop] = round(us_time_value * s_from_us, 6)

            btb_word_number_match = export_regex.match(line)
            if btb_word_number_match:
                print(f"matched line {line!r}")
                btb_word_number = int(btb_word_number_match[1])
                if "bt-export" not in result:
                    result["bt-export"] = {}
                result["bt-export"]["btb_words"] = btb_word_number

    return result

def print_timespans(timespans, csv_file = None):
    if csv_file is not None:
        print("trace_interval,main_app,part_app,start,stop", file = csv_file)

    for trace_interval, ts1 in timespans.items():
        for main_app, ts2 in ts1.items():
            # sanity checks
            if "backtracer" not in ts2:
                raise KeyError(
                    f"{trace_interval = }, {main_app = } has no backtracer span"
                )
            bt = ts2["backtracer"]
            ex = ts2["bt-export"]
            for part_app in {
                part_app
                for part_app in ts2.keys()
                if part_app != "backtracer" and part_app != "bt-export"
            }:
                real = ts2[part_app]
                assert bt["start"] <= real["start"] < real["stop"] <= bt["stop"]
            assert bt["stop"] < ex["start"] < ex["stop"]

            for part_app, span in ts2.items():
                start = span["start"]
                stop = span["stop"]
                print(
                    f"{trace_interval:6.3f} s, {main_app:20}: "
                    f"{part_app:20} ran {start:8.3f} .. {stop:8.3f} "
                    f"-> {stop - start:8.3f}"
                )

                if csv_file is not None:
                    print(
                        f"{trace_interval},{main_app},{part_app},{start},{stop}",
                        file = csv_file
                    )

def print_btb_words(timespans, csv_file = None):
    if csv_file is not None:
        print("trace_interval,app,btb_words", file = csv_file)

    for trace_interval, ts1 in timespans.items():
        for app, ts2 in ts1.items():
            ex = ts2["bt-export"]
            btb_words = ex["btb_words"]

            print(
                f"{trace_interval:6.3f} s, {app:20}: "
                f"wrote {btb_words:7} words (u64) of btb"
            )

            if csv_file is not None:
                print(
                    f"{trace_interval},{app},{btb_words}",
                    file = csv_file
                )

def plot(csv_filename):
    import pandas as pd
    import seaborn as sns
    from matplotlib import pyplot as plt

    df = pd.read_csv(csv_filename)
    data = df.query("part_app != 'backtracer' and part_app != 'bt-export'")
    duration = data["stop"] - data["start"]
    data.insert(1, "duration", duration)
    sns.barplot(
        data = data,
        x = "part_app",
        y = "duration",
        hue = "trace_interval",
    )
    print(data)
    plt.savefig("data/app_duration.svg")

def plot_btb_words(csv_filename):
    import pandas as pd
    import seaborn as sns
    from matplotlib import pyplot as plt

    data = pd.read_csv(csv_filename)
    sns.barplot(
        data = data,
        x = "app",
        y = "btb_words",
        hue = "trace_interval",
    )
    plt.savefig("data/btb_words.svg")

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

    timespans = {
        trace_interval: {
            app: {}
            for app in args.apps
        }
        for trace_interval in args.trace_intervals
    }
    for trace_interval in args.trace_intervals:
        write_measure_defaults(
            s_sleep_before_tracing = args.sleep_before_tracing,
            s_trace_interval = trace_interval,
            do_overhead = args.overhead,
            do_export = args.flamegraph,
        )
        for app in args.apps:
            timespans[trace_interval][app] = measure_overhead(
                app,
                int(1000_000 * trace_interval),
                args
            )

    csv_filename = "data/timespans.csv"
    with open(csv_filename, "w") as csv_file:
        print_timespans(timespans, csv_file)

    if args.plot:
        plot(csv_filename)

    btb_words_filename = "data/btb_words.csv"
    with open(btb_words_filename, "w") as csv_file:
        print_btb_words(timespans, csv_file)

    if args.plot:
        plot_btb_words(btb_words_filename)


if __name__ == "__main__":
    main()
