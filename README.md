# L4Re Userspace and External Backtracer

This package contains the tools to interface with the Fiasco JDB Backtracing
including exporting the results.

# Installing / Packages

For an example on how to install all needed packages, see `install.sh`. Adapt and use
what you need in an existing L4Re development environment. In case there's a bug,
a look at the structures in
[my docker- and submodule-based version](https://gitlab.hrz.tu-chemnitz.de/anob943c-at-tu-dresden.de/fl4mer)
might be useful, using not branch `fl4mer` but `fl4mer-antonia`, which uses git submodules.

# Documentation

This package is not currently part of the L4Re operating system.
This is the documentation:

## Getting a trace

Build your L4Re with the package [backtracer](https://github.com/AntoniaObersteiner/backtracer)
and add it as a module to a copy of your `<...>.cfg`, that is called `<...>-backtracer.cfg`.
**(this is stupid, change this)**
You might want to set `backtracer/include/measure_defaults.h:app_controls_tracing = 0;`
so that the backtracer just waits for the specified `us_sleep_before_tracing` and then starts tracing.
The tracing is done in the kernel, so the tracer sleeps
for `us_backtracer_waits_for_app` and then stops the backtracer.
**(continue)**

Currently, there is no network support, so you have to rely on the serial console.

## Usage example

Currently, this is the directory structure:
```
./server/     # L4 "server" that controls backtrace exporting
./include/    # implements system call wrappers and performance measurement
./include/measure_defaults.h  # configures server behavior
./external/   # tools for processing and analyzing stack traces
```

### ./external
This project is controlled via Make, the Makefile is renamed to `External.make`
to avoid conflict with L4Re's build system

The results are written to `data/`, usually in a subdirectory, that currently has to be explicitly named via `LABEL=`.
```bash
make LABEL=test data/test/hello.svg
# equivalent to
make LABEL=test MODULE=hello ENDING=svg default
```

Possible endings that can be substituted for `svg` above:

- `traced`: runs the docker, collects the output
- `cleaned`: removes control characters from `traced`
	- data written as hex u64 somewhere in text with `>=<` to mark the lines
	- data has binary format with (usually 1KiB) blocks (simple and not yet very useful XOR redundancy)
- `compressed`: export blocks from `cleaned` now contiguous without redundancy blocks
	- still with dictionary compression on larger blocks (usually 8KiB)
- `btb`: decompressed backtrace buffer binary format as written inside the JDB BTB Kernel implementation
- `interpreted`: human readable version of BTB format
- `folded`: line-for-line stack-traces, input for FlameGraph
- `svg`: output of FlameGraph
- `log`: makes all of the above and does not delete intermediate files

### how to select the traced program

`make -f External.make data/$(MODULE).svg` will start ned with the config `$(MODULE)-backtraced.cfg`.

## Future Delvopment

- add any other timer
- intermediate-save symbol tables (or binaries)
    - only relevant table parts in file
- more documentation
- very deep stack fix
- refactor fiasco (jdb_bt and jdb_btb)
- clean up inner bt debugging
- clean up inner bt preprocessor directives
- clean up Kconfig
- make various timers configurable
- clean up backtracer directories
- clean up backtracer Makefile (switch to different system?)
- clean up backtracer block system
- network export?
    - Till M nach UDP-Code fragen
    - luna, incl. Beispielprogramm
    - qemu nic-dev-type=e1000 oder so
    - tune-?tab-device verbindet sich in qemu, auf host dann mit bridge

