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

def main():
    print(f"reading file {args.filename!r}")

    data = pd.read_csv(args.filename)

    # convert to µs
    unit = "µs"
    unit_factor = 1000
    data[f"average_time_in_{unit}"] = data["average_time_in_ns"] / unit_factor

    # use only the first exported histogram
    data = data.query("hist_counter == 0")
    # remove empty bins from the end
    max_depth_min = data.query("count > 0")["depth_min"].max()
    data = data.query(f"depth_min <= {max_depth_min}")
    print(data)

    # make sure the x-axis ticks are integer and rougly 10
    x_plot_width = data.shape[0]
    tick_step = x_plot_width // 11 + 1

    app = os.path.basename(args.filename).split(".")[0]
    de = {
        "title": f"Dauer je Trace in Anwendung {app}",
        "xlabel": f"Stack-Tiefe in Frames",
        "rlabel": f"Durchschnittliche Dauer in {unit}",
        "llabel": f"Anzahl mit dieser Stacktiefe",
    }
    en = {
        "title": f"duration per tracing of application {app}",
        "xlabel": f"stack depth in frames",
        "rlabel": f"average trace time in {unit}",
        "llabel": f"number of traces of this depth",
    }
    lang = de

    sns.set_style("whitegrid")

    # plot backtround histogram of how many stacks per bin
    ax = sns.barplot(
        data = data,
        x = "depth_min",
        y = "count",
        label = lang["llabel"],
    )

    sns.set_style("white")
    # make a second plot in front for average duration
    ax_twin = ax.twinx()
    lines_twin = sns.scatterplot(
        data = data.query("count != 0"),
        x = "depth_min",
        y = f"average_time_in_{unit}",
        ax = ax_twin,
        color = "black",
        label = lang["rlabel"],
    )

    ax.xaxis.set_major_locator(mticker.MultipleLocator(tick_step))

    ax.set_title(lang["title"])
    ax.set_xlabel(lang["xlabel"])
    ax.set_ylabel(lang["llabel"])
    ax_twin.set_ylabel(lang["rlabel"])
    ax.set_ymargin(.3)

    h1,l1 = ax     .get_legend_handles_labels()
    h2,l2 = ax_twin.get_legend_handles_labels()
    ax_twin.legend(
        handles = h1 + h2,
        labels  = l1 + l2,
    )
    ax.get_legend().remove()

    output_filename = args.filename + ".svg"
    print(f"writing plot to {output_filename!r}")
    plt.savefig(output_filename)

if __name__ == "__main__":
    main()
