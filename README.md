# L4Re Userspace and External Backtracer

This package contains the tools to interface with the Fiasco JDB Backtracing
including exporting the results.

# Documentation

This package is not currently part of the L4Re operating system.
This is the documentation:

## Usage example

The directory structure is not final. Currently, we live here:
```bash
cd server/src
```

This project is controlled via Make, the Makefile is renamed to `External.make`
to avoid conflict with L4Re's build system

The results are written to `data/`, optionally in subdirectories, that currently have to be explicitly named via `LABEL=`.
```bash
make -f External.make data/qsort.svg
make -f External.make LABEL=test data/test/qsort.svg
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
- more documentation
- refactor fiasco (jdb_bt and jdb_btb)
- clean up inner bt debugging
- clean up inner bt preprocessor directives
- clean up Kconfig
- make various timers configurable
- clean up backtracer directories
- clean up backtracer Makefile (switch to different system?)
- clean up backtracer block system
- network export?

