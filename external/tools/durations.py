#!/bin/python3
import argparse

argparser = argparse.ArgumentParser()

argparser.add_argument(
    "filename",
    help = "what file to read. should be a .durations from ../interpret",
)

if __name__ == "__main__":
    args = argparser.parse_args()

import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
# since some moving round of files, Hist.py has been lost, rendering durations.py broken
# Hist.py implemented Hist, a Histogram class that could find the bin range that included
# a certain percentage of counted samples
from Hist import Hist

def hisplot(data, binrange, binwidth, label):
    plotted = pd.DataFrame({
        "µs_interval": data["ns_interval"] / 1000.0,
        "µs_duration": data["ns_duration"] / 1000.0,
    })

    # with what data do we do range setting and similar
    hist_data = plotted["µs_interval"]
    if binrange == "auto":
        hist, bins = np.histogram(hist_data, 100)
        start, end = Hist(bins, hist).span_bins()
        binrange = (bins[start], bins[end])
        print(f"{binrange = }")
    if binrange is None:
        binrange = (min(hist_data), max(hist_data))

    rangewidth = binrange[1] - binrange[0]
    hist, bins = np.histogram(hist_data, bins = int(rangewidth / binwidth), range = binrange)
    sns.histplot(
        data = plotted,
        binrange = binrange,
        binwidth = binwidth,
    )
    ax = plt.gca()
    ax.set_ybound(-.05 * max(1, max(hist)), None)
    newfilename = ".".join(args.filename.split(".")[:-1]) + f".runnings{label}.svg"
    print(f"{newfilename = !r}")
    plt.savefig(newfilename)

def main():
    data = pd.read_csv(args.filename)
    hisplot(data, (0,    300),   1, ".narrow")
    hisplot(data,       None,  100, "")
    hisplot(data,     "auto",  100, ".flex")

if __name__ == "__main__":
    main()
