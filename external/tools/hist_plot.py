#!/usr/bin/python3

import argparse

argparser = argparse.ArgumentParser()
argparser.add_argument(
    "filename",
)
argparser.add_argument(
    "--lang",
    default = "en",
    help = "which language, currently 'de', 'en' (default)",
)
argparser.add_argument(
    "--do-min",
    default = True,
    help = "do estimation on minima per group",
)

if __name__ == "__main__":
    args = argparser.parse_args()

import os
import pandas as pd
import numpy as np
import seaborn as sns
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import matplotlib.patches as mpatches
from copy import deepcopy

def make_estimator(data, unit):
    counts    = data["count"]
    depths    = data["depth_min"]
    durations = data[f"average_time_in_{unit}"]

    X = (depths    * counts).sum() / counts.sum()
    Y = (durations * counts).sum() / counts.sum()
    variance = (counts * (depths - X)**2).sum()
    b = (counts * (depths - X) * (durations - Y)).sum() / variance if variance != 0 else 0
    a = Y - b * X
    linear_estimator = lambda depth: a + b * depth

    estimations = linear_estimator(depths)
    error =  (counts          * (estimations - durations)**2).sum() / counts.sum()
    derr_a = (counts          * (estimations - durations)).sum() / counts.sum()
    derr_b = (counts * depths * (estimations - durations)).sum() / counts.sum()
    print(f"linear regession duration (depth) = {a: 8.6f} {b:+8.6f} * depth")
    print(f" ⇒ MSE:      {error: 8.6f}")
    print(f" ⇒ ∂err/∂a = {derr_a: 8.6f}")
    print(f" ⇒ ∂err/∂b = {derr_b: 8.6f}")

    description = f"${b:.3f}·depth{a:+.3f}$"

    return linear_estimator, description

def make_estimator_from_raw(data, unit):
    if args.do_min:
        grouped = data.groupby("stack_depth")
        selected = grouped[f"{unit}_duration"]
        minima = selected.min()
        # print(f"{grouped = }")
        # print(f"{selected = }")
        print(f"{minima = }")
        print(f"{len(minima) = }")
        print(f"{len(minima.index) = }")
        data = pd.DataFrame(data = {
            f"{unit}_duration": list(minima),
            "stack_depth": list(minima.index),
        })
    depths    = data["stack_depth"]
    durations = data[f"{unit}_duration"]
    print(f"{depths = }")
    print(f"{durations = }")

    total_count = depths.shape[0]

    X = depths   .sum() / total_count
    Y = durations.sum() / total_count
    variance = ((depths - X)**2).sum()
    b = ((depths - X) * (durations - Y)).sum() / variance if variance != 0 else 0
    a = Y - b * X
    linear_estimator = lambda depth: a + b * depth

    estimations = linear_estimator(depths)
    error =  (         (estimations - durations)**2).sum() / total_count
    derr_a = (         (estimations - durations)).sum() / total_count
    derr_b = (depths * (estimations - durations)).sum() / total_count
    print(f"linear regession duration (depth) = {a: 8.6f} {b:+8.6f} * depth")
    print(f" ⇒ MSE:      {error: 8.6f}")
    print(f" ⇒ ∂err/∂a = {derr_a: 8.6f}")
    print(f" ⇒ ∂err/∂b = {derr_b: 8.6f}")

    description = f"${b:.3f}·depth{a:+.3f}$"

    return linear_estimator, description

def make_lang(unit_text):
    app = os.path.basename(args.filename).split(".")[0]
    de = {
        "title": f"Dauer je Trace in Anwendung {app}",
        "xlabel": f"Stack-Tiefe in Frames",
        "rlabel": f"Dauer in {unit_text}",
        "elabel": f"Lineare Näherung der Dauer",
        "llabel": f"Anzahl mit dieser Stacktiefe  ",
        "depth": "Stack-Tiefe",
    }
    en = {
        "title": f"duration per tracing of application {app}",
        "xlabel": f"stack depth in frames",
        "rlabel": f"trace time in {unit_text}",
        "elabel": f"linear approximation of the durations",
        "llabel": f"number of traces of this depth              ",
        "depth": "depth",
    }
    if args.lang == "de":
        return de
    elif args.lang == "en":
        return en
    raise ValueError(f"unknown language '{args.lang}'")

def do_style(legend_facecolor = None):
    try:
        plt.style.use('seaborn-v0_8')
    except OSError:
        plt.style.use('seaborn')
    # print(plt.rcParams.keys())
    params = {
        "text.usetex" : False,
        #"font.family" : "serif",
        "font.serif" : ["Computer Modern Serif"] + plt.rcParams["font.serif"],
        "font.sans-serif" : ["TeX Gyre Schola"] + plt.rcParams["font.sans-serif"],
        "legend.facecolor": legend_facecolor if legend_facecolor is not None else "inherit",
        "legend.frameon": legend_facecolor != None,
    }
    plt.rcParams.update(params)

def from_raw_durations():
    print(f"reading file {args.filename!r}")

    data = pd.read_csv(args.filename)

    # convert to µs
    unit = "µs"
    unit_text = "µs"
    unit_factor = 1000
    data[f"{unit}_duration"] = data["ns_duration"] / unit_factor

    # remove empty bins from the end
    max_depth = data["stack_depth"].max()
    min_depth = data["stack_depth"].min()

    linear_estimator, linear_estimator_description = make_estimator_from_raw(data, unit)

    # make sure there are roughly 10 x-axis ticks
    tick_step = (max_depth - min_depth) // 11 + 1

    lang = make_lang(unit_text)
    do_style(legend_facecolor = "white")

    def boxplot_or_scatter_plot(ax = None, visible = True):
        if max_depth < 100:
            return sns.boxplot(
                data = data,
                x = data["stack_depth"] - data["stack_depth"].min(),
                y = f"{unit}_duration",
                color = "orange" if visible else (0, 0, 0, 0),
                width = .6,
                ax = ax,
            )
        else:
            return sns.scatterplot(
                data = data,
                x = data["stack_depth"] - data["stack_depth"].min(),
                y = f"{unit}_duration",
                color = "orange" if visible else (0, 0, 0, 0),
                ax = ax,
                marker = "+",
            )


    # make background, so grid is behind bars but matches duration ticks
    ax_bg = boxplot_or_scatter_plot(visible = False)

    depths = np.array(range(min_depth, max_depth + 1))
    counts = [
        data.query(f"stack_depth == {depth}").shape[0]
        for depth in depths
    ]
    # plot backtround histogram of how many stacks per bin
    ax_bar = ax_bg.twinx()
    sns.barplot(
        x = depths,
        y = counts,
        ax = ax_bar,
        label = lang["llabel"],
    )
    xlim = ax_bar.get_xlim()
    xlim_diff = (xlim[1] - xlim[0])**.5 / 10
    xlim = (xlim[0] - xlim_diff, xlim[1] + xlim_diff)

    layout_and_export(
        ax_bg,
        ax_bar,
        ax_est = DummyAxes(),
        ax_dot = DummyAxes(),
        lang = lang,
        tick_step = tick_step,
        linear_estimator_description = linear_estimator_description,
        filename_extra = ".bars",
        xlim = xlim,
    )

    # plot actual durations
    ax_dot = ax_bg.twinx()
    boxplot_or_scatter_plot(ax = ax_dot)

    layout_and_export(
        ax_bg,
        ax_bar,
        ax_est = DummyAxes(),
        ax_dot = ax_dot,
        lang = lang,
        tick_step = tick_step,
        linear_estimator_description = linear_estimator_description,
        filename_extra = ".dot",
        xlim = xlim,
    )

    # make another plot in front for linear regression
    ax_est = ax_bg.twinx()
    sns.lineplot(
        x = depths - min_depth,
        y = linear_estimator(depths),
        ax = ax_est,
        color = "black",
        label = lang["elabel"],
    )

    #sns.set_style("white")
    # make another plot in front for actual durations
    ax_dot = ax_bg.twinx()
    boxplot_or_scatter_plot(ax = ax_dot)

    max_duration_estimate = linear_estimator(max_depth)
    ax_dot.set_ylim(top = max(40, int(1.3 * max_duration_estimate)))

    layout_and_export(
        ax_bg,
        ax_bar,
        ax_est,
        ax_dot,
        lang,
        tick_step,
        linear_estimator_description,
        xlim = xlim,
    )

class DummyAxis:
    def __init__(self): pass
    def set_major_locator(self, arg): pass
    def set_label_position(self, arg): pass

class DummyLegend:
    def __init__(self): pass
    def remove(self): pass
    def set_label_position(self, arg): pass

class DummyAxes:
    def __init__(self):
        self.xaxis = DummyAxis()
        self.yaxis = DummyAxis()
    def grid(self, arg): pass
    def set_title(self, arg): pass
    def set_ybound(self, arg): pass
    def get_ybound(self): pass
    def set_xlim(self, arg): pass
    def set_ylim(self, **kwargs): pass
    def set_xlabel(self, arg): pass
    def set_ylabel(self, arg): pass
    def set_ylabel(self, arg): pass
    def tick_params(self, **kwargs): pass
    def get_legend_handles_labels(self): return [], []
    def legend(self, **kwargs): pass
    def get_legend(self): return DummyLegend()

def layout_and_export(
    ax_bg,
    ax_bar,
    ax_est,
    ax_dot,
    lang,
    tick_step,
    linear_estimator_description,
    *,
    filename_extra = "",
    xlim = None,
):
    if xlim is None:
        xlim = ax_bar.get_xlim()
    ax_bar.set_ymargin(.3)
    ax_dot.set_ylim(bottom = 0)
    ax_bar.xaxis.set_major_locator(mticker.MultipleLocator(tick_step))
    ax_dot.set_ybound(ax_bg.get_ybound())
    ax_est.set_ybound(ax_bg.get_ybound())
    ax_bg .set_xlim(xlim)
    ax_dot.set_xlim(xlim)
    ax_est.set_xlim(xlim)
    ax_bg .grid(True)
    ax_est.grid(False)
    ax_dot.grid(False)
    ax_bar.grid(False)

    label_dot = not isinstance(ax_dot, DummyAxes)
    label_est = not label_dot and not isinstance(ax_est, DummyAxes)
    label_bg  = not label_dot and not label_est
    ax_bg .set_title(lang["title"])
    ax_bg .set_xlabel(lang["xlabel"] + "\n" + lang["elabel"] + ": " + linear_estimator_description)
    ax_bar.set_ylabel(lang["llabel"])
    ax_bg .set_ylabel(lang["rlabel"] if label_bg  else "")
    ax_dot.set_ylabel(lang["rlabel"] if label_dot else "")
    ax_est.set_ylabel(lang["rlabel"] if label_est else "")
    plt.subplots_adjust(bottom = .15)
    ax_bar.tick_params(axis = "y", left = True,  right = False,     labelleft = True,  labelright = False)
    ax_bg .tick_params(axis = "y", left = False, right = label_bg,  labelleft = False, labelright = label_bg)
    ax_est.tick_params(axis = "y", left = False, right = label_est, labelleft = False, labelright = label_est)
    ax_dot.tick_params(axis = "y", left = False, right = label_dot, labelleft = False, labelright = label_dot)
    ax_bar.yaxis.set_label_position("left")
    ax_bg .yaxis.set_label_position("right")
    ax_dot.yaxis.set_label_position("right")
    ax_est.yaxis.set_label_position("right")

    h1,l1 = ax_bar.get_legend_handles_labels()
    h2,l2 = ax_dot.get_legend_handles_labels()
    if not h2 and not isinstance(ax_dot, DummyAxes):
        h2 = [mpatches.Patch(color = "orange")]
        l2 = [lang["rlabel"]]
    if ax_bar.get_legend() is not None:
        ax_bar.get_legend().remove()
    if ax_est.get_legend() is not None:
        ax_est.get_legend().remove()

    h3,l3 = ax_est.get_legend_handles_labels()
    print(f"{h1 = }, {l1 = }")
    print(f"{h2 = }, {l2 = }")
    print(f"{h3 = }, {l3 = }")
    ax_for_legend = ax_dot if not isinstance(ax_dot, DummyAxes) else ax_bar
    ax_for_legend.legend(
        handles = h1 + h2 + h3,
        labels  = l1 + l2 + l3,
        loc = "upper right",
    )

    for ending in ["svg", "pdf"]:
        output_filename = args.filename + filename_extra + "." + ending
        print(f"writing plot to {output_filename!r}")
        plt.savefig(output_filename)

    if ax_for_legend.get_legend() is not None:
        ax_for_legend.get_legend().remove()

def from_histogram():
    print(f"reading file {args.filename!r}")

    data = pd.read_csv(args.filename)

    # convert to µs
    unit = "µs"
    unit_text = "µs"
    unit_factor = 1000
    data[f"average_time_in_{unit}"] = data["average_time_in_ns"] / unit_factor

    # use only the first exported histogram
    data = data.query("hist_counter == 0")
    # remove empty bins from the end
    min_depth_min = data.query("count > 0")["depth_min"].min()
    max_depth_min = data.query("count > 0")["depth_min"].max()
    if str(max_depth_min) == "nan":
        print("can't produce a plot, there is no data?")
        return
    data = data.query(f"depth_min <= {max_depth_min}")
    print(data)

    remove_outliers = False
    linear_estimator, linear_estimator_description = make_estimator(data, unit)
    if remove_outliers:
        estimations = linear_estimator(data["depth_min"])
        # the decision, what is and isn't an outlier is very unmethodic
        non_outlier_data = data[(data[f"average_time_in_{unit}"] - estimations).abs() <= 1000 / unit_factor]
        linear_estimator, linear_estimator_description = make_estimator(non_outlier_data, unit)

    # make sure the x-axis ticks are integer and rougly 10
    x_plot_width = data.shape[0]
    tick_step = x_plot_width // 11 + 1

    lang = make_lang(unit_text)
    do_style()

    # make background, so grid is behind bars but matches duration ticks
    ax_bg = sns.scatterplot(
        data = data.query("count != 0"),
        x = data["depth_min"] - min_depth_min,
        y = f"average_time_in_{unit}",
        color = (0, 0, 0, 0),
    )

    # plot background histogram of how many stacks per bin
    ax_bar = ax_bg.twinx()
    sns.barplot(
        data = data,
        x = "depth_min",
        y = "count",
        ax = ax_bar,
        label = lang["llabel"],
    )
    xlim = ax_bar.get_xlim()
    xlim_diff = (xlim[1] - xlim[0])**.5 / 10
    xlim = (xlim[0] - xlim_diff, xlim[1] + xlim_diff)

    layout_and_export(
        ax_bg,
        ax_bar,
        ax_est = DummyAxes(),
        ax_dot = DummyAxes(),
        lang = lang,
        tick_step = tick_step,
        linear_estimator_description = linear_estimator_description,
        filename_extra = ".bars",
        xlim = xlim,
    )

    # plot average duration
    ax_dot = ax_bg.twinx()
    sns.scatterplot(
        data = data.query("count != 0"),
        x = data["depth_min"] - min_depth_min,
        y = f"average_time_in_{unit}",
        ax = ax_dot,
        color = "black",
        label = lang["rlabel"],
    )

    layout_and_export(
        ax_bg,
        ax_bar,
        ax_est = DummyAxes(),
        ax_dot = ax_dot,
        lang = lang,
        tick_step = tick_step,
        linear_estimator_description = linear_estimator_description,
        filename_extra = ".dot",
        xlim = xlim,
    )

    ax_dot.remove()

    # make another plot in front for linear regression
    ax_est = ax_bg.twinx()
    sns.lineplot(
        x = data["depth_min"],
        y = linear_estimator(data["depth_min"]),
        ax = ax_est,
        color = "black",
        label = lang["elabel"],
    )

    # make another plot in front for average duration
    ax_dot = ax_bg.twinx()
    sns.scatterplot(
        data = data.query("count != 0"),
        x = data["depth_min"] - min_depth_min,
        y = f"average_time_in_{unit}",
        ax = ax_dot,
        color = "black",
        label = lang["rlabel"],
    )

    layout_and_export(
        ax_bg,
        ax_bar,
        ax_est,
        ax_dot,
        lang,
        tick_step,
        linear_estimator_description,
        xlim = xlim,
    )

def main():
    if args.filename.endswith(".histogram"):
        from_histogram()
    elif args.filename.endswith(".durations"):
        from_raw_durations()
    else:
        raise ValueError(f"unknown file ending of {args.filename = !r}")

if __name__ == "__main__":
    main()
