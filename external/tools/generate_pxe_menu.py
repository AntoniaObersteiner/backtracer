#!/usr/bin/python3

import argparse

argparser = argparse.ArgumentParser(
)

argparser.add_argument(
    "--bins",
    nargs = "*",
    help = "the binaries for the boot directory, aka modules",
)
argparser.add_argument(
    "--conf",
    nargs = "*",
    help = "the config files for the boot directory, also loaded like modules (?)",
)
argparser.add_argument(
    "--output-filename",
    default = "./menu.lst",
    help = "where to write the resulting boot menu list",
)

def generate_entry(conf, bins, confs):
    title = ".".join(conf.split(".")[:-1]) # everything except the (.conf) ending

    output = ""
    output += f"title {title}\n"
    output += f"kernel $(BOOT)/bootstrap -modaddr 0x01100000\n"
    output += f"module $(BOOT)/fiasco -serial_esc\n"
    output += f"module $(BOOT)/moe rom/{conf}\n"
    output += f"module $(BOOT)/l4re\n"
    output += f"module $(BOOT)/{conf}\n"

    bins = set(bins)
    bins -= {"fiasco", "bootstrap", "moe", "l4re"}
    bins = list(bins)
    bins.sort()

    for bin in bins:
        output += f"module $(BOOT)/{bin}\n"

    return output

menu_header = """
color 23 52
default 0
timeout 10

set BOOT=(nd)/tftpboot/aoberst
"""

def main():
    args = argparser.parse_args()

    bins = list({
        bin.split("/")[-1]
        for bin in args.bins
    })
    confs = list({
        bin.split("/")[-1]
        for bin in args.conf
    })

    output = menu_header
    for number, conf in enumerate(confs):
        output += f"#{number}\n"
        output += generate_entry(conf, bins, confs)
        output += "\n"

    print(f"writing to {args.output_filename = !r}")
    with open(args.output_filename, "w") as output_file:
        output_file.write(output)

if __name__ == "__main__":
    main()
