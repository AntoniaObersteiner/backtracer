#!/usr/bin/python3

import argparse

argparser = argparse.ArgumentParser()
argparser.add_argument(
    "filename",
)
args = argparser.parse_args()

import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker

def main():
    print(f"reading file {args.filename!r}")

    data = pd.read_csv(args.filename)

    data = data.query("hist_counter == 1")
    data = data.query("count != 0")
    sns.scatterplot(
        data = data,
        x = "depth_min",
        y = "average_time_in_ns",
    )
    result_count = data.shape[0]
    tick_step = result_count // 11 + 1
    plt.gca().xaxis.set_major_locator(mticker.MultipleLocator(tick_step))
    output_filename = args.filename + ".svg"
    print(f"writing plot to {output_filename!r}")
    plt.savefig(output_filename)

if __name__ == "__main__":
    main()
