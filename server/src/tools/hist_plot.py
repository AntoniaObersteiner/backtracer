#!/usr/bin/python3

import argparse

argparser = argparse.ArgumentParser()
argparser.add_argument(
    "filename",
)

if __name__ == "__main__":
    args = argparser.parse_args()

import os
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker

def make_estimator(data, unit):
    counts    = data["count"]
    depths    = data["depth_min"]
    durations = data[f"average_time_in_{unit}"]

    X = (depths    * counts).sum() / counts.sum()
    Y = (durations * counts).sum() / counts.sum()
    b = (counts * (depths - X) * (durations - Y)).sum() / (counts * (depths - X)**2).sum()
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

def main():
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
    max_depth_min = data.query("count > 0")["depth_min"].max()
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

    app = os.path.basename(args.filename).split(".")[0]
    de = {
        "title": f"Dauer je Trace in Anwendung {app}",
        "xlabel": f"Stack-Tiefe in Frames",
        "rlabel": f"Durchschnittliche Dauer in {unit_text}",
        "elabel": f"Lineare Näherung der Dauer",
        "llabel": f"Anzahl mit dieser Stacktiefe",
        "depth": "Stack-Tiefe",
    }
    en = {
        "title": f"duration per tracing of application {app}",
        "xlabel": f"stack depth in frames",
        "rlabel": f"average trace time in {unit_text}",
        "elabel": f"linear approximation of the durations",
        "llabel": f"number of traces of this depth",
        "depth": "depth",
    }
    lang = de

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
    }
    plt.rcParams.update(params)

    # make background, so grid is behind bars but matches duration ticks
    ax_bg = sns.scatterplot(
        data = data.query("count != 0"),
        x = "depth_min",
        y = f"average_time_in_{unit}",
        color = "black",
    )

    # plot backtround histogram of how many stacks per bin
    ax_bar = ax_bg.twinx()
    sns.barplot(
        data = data,
        x = "depth_min",
        y = "count",
        ax = ax_bar,
        label = lang["llabel"],
    )

    # make another plot in front for linear regression
    ax_est = ax_bg.twinx()
    sns.lineplot(
        x = data["depth_min"],
        y = linear_estimator(data["depth_min"]),
        ax = ax_est,
        color = "black",
        label = lang["elabel"],
    )

    #sns.set_style("white")
    # make another plot in front for average duration
    ax_dot = ax_bg.twinx()
    sns.scatterplot(
        data = data.query("count != 0"),
        x = "depth_min",
        y = f"average_time_in_{unit}",
        ax = ax_dot,
        color = "black",
        label = lang["rlabel"],
    )

    ax_bar.set_ymargin(.3)
    ax_bar.xaxis.set_major_locator(mticker.MultipleLocator(tick_step))
    ax_bg .set_ybound(ax_dot.get_ybound())
    ax_est.set_ybound(ax_dot.get_ybound())
    ax_bg .grid(True)
    ax_est.grid(False)
    ax_dot.grid(False)
    ax_bar.grid(False)

    ax_bg .set_title(lang["title"])
    ax_bg .set_xlabel(lang["xlabel"] + "\n" + lang["elabel"] + ": " + linear_estimator_description)
    ax_bar.set_ylabel(lang["llabel"])
    ax_dot.set_ylabel(lang["rlabel"])
    ax_bg .set_ylabel("")
    ax_est.set_ylabel("")
    plt.subplots_adjust(bottom = .15)
    ax_bg .tick_params(axis = "y", left = False, right = False, labelleft = False, labelright = False)
    ax_bar.tick_params(axis = "y", left = True,  right = False, labelleft = True,  labelright = False)
    ax_est.tick_params(axis = "y", left = False, right = False, labelleft = False, labelright = False)
    ax_dot.tick_params(axis = "y", left = False, right = True,  labelleft = False, labelright = True)
    ax_bar.yaxis.set_label_position("left")
    ax_dot.yaxis.set_label_position("right")

    h1,l1 = ax_bar.get_legend_handles_labels()
    h2,l2 = ax_dot.get_legend_handles_labels()
    h3,l3 = ax_est.get_legend_handles_labels()
    ax_dot.legend(
        handles = h1 + h2 + h3,
        labels  = l1 + l2 + l3,
    )
    try:
        ax_bar.get_legend().remove()
        ax_est.get_legend().remove()
    except:
        pass

    for ending in ["svg", "pdf"]:
        output_filename = args.filename + "." + ending
        print(f"writing plot to {output_filename!r}")
        plt.savefig(output_filename)

if __name__ == "__main__":
    main()
