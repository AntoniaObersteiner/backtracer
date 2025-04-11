#!/bin/python3
import argparse

argparser = argparse.ArgumentParser()

argparser.add_argument(
    "filename",
    help = "what file to read. should be a .durations from ../interpret",
)

if __name__ == "__main__":
    args = argparser.parse_args()

import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

def hisplot(data, binrange, binwidth, label):
    plotted = pd.DataFrame({
        "µs_interval": data["ns_interval"] / 1000.0,
        "µs_duration": data["ns_duration"] / 1000.0,
    })
    sns.histplot(
        data = plotted,
        binrange = binrange,
        binwidth = binwidth,
    )
    ax = plt.gca()
    ax.set_ybound(-10, None)
    newfilename = ".".join(args.filename.split(".")[:-1]) + f".runnings{label}.svg"
    print(f"{newfilename = !r}")
    plt.savefig(newfilename)

def main():
    data = pd.read_csv(args.filename)
    hisplot(data, (0,    300),   1, ".narrow")
    hisplot(data,       None,  100, "")

if __name__ == "__main__":
    main()
