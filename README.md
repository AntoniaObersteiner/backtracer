# L4Re Userspace and External Backtracer

This package contains the tools to interface with the Fiasco JDB Kernel Backtracer
including exporting the results.
The Kernel Backtracer traces both user and kernel stacks, as long as the timer can interrupt the execution.

# Quick Start

For an example on how to install all needed packages, build and run with the default configuration,
see `install.sh`. It can be run in an empty directory and should result in a FrameGraph of hello.
It needs your input twice: First, in the L4Re and Kernel Config: Leave L4Re at the defaults
and use the following Fiasco Kernel Config:

- no [P]erformance configuration
- no [C]ompile without frame pointer
- [A]llow tracing user and kernel stacks

Secondly, it needs you to stop qemu, when the export is done. It tells you :)

Adapt and use what you need for an existing L4Re development environment.
In case there's a bug, ask [me](mailto:antonia.obersteiner@tu-dresden.de).
Maybe a look at
[my docker- and submodule-based version](https://gitlab.hrz.tu-chemnitz.de/anob943c-at-tu-dresden.de/fl4mer)
might be useful, using not branch `fl4mer` but `fl4mer-antonia`.

# Documentation

This package is not currently part of the L4Re operating system.
This is the documentation:

## Getting a Trace

Build your L4Re with the package [backtracer](https://github.com/AntoniaObersteiner/backtracer)
and add it as a module to your `<...>.cfg`.
If you want to work with `Antonia.make`, call your config `<...>-backtracer.cfg`.
You might want to set `backtracer/include/measure_defaults.h:app_controls_tracing = 0`
so that the backtracer is time-controlled.
It first waits `us_sleep_before_tracing` µs, starts the tracing, then
waits for `us_backtracer_waits_for_app` µs and then stops the backtracer and exports.

### App-Control

Alternatively, set `app_controls_tracing = 1` and start and stop the kernel-tracer from your application
using the debug-capability. This would be a minimal example:

```cpp
#include <stdio.h>
#include <l4/backtracer/btb_control.h>  // defines l4_debugger_backtracing_*
#include <l4/backtracer/measure.h> // defines dbg_cap default

int main(void) {
	l4_debugger_backtracing_start(dbg_cap);
	for (int i = 0; i < 100; i++) {
		puts("Hello World");
	}
	l4_debugger_backtracing_stop(dbg_cap);
}
```

#### Mechanism of App-Controlled Tracing

The backtracer -- when configured with `app_controls_tracing == 1` -- checks the number of words in the backtrace buffer.
Once that number changes, it waits until the kernel reports that tracing was stopped and starts exporting.
Direct control of the app over the backtracer is planned.

It may happen that the backtracer is not scheduled until the traced program(s) have already stopped the tracing,
and thus does not stop tracing normally.
If this is a problem, set `backtracer/include/measure_defaults.h:rounds_backtracer_waits_for_start = 1`
or another small number so it doesn't wait for 20 rounds (=20 seconds), as is the default.

Currently, there is no network support, so you have to rely on the serial console export.
Capture the print-out to a `.traced` file.

## Processing the Sample

Currently, this is the directory structure:
```
./server/     # L4 "server" that controls backtrace exporting
./include/    # implements system call wrappers and performance measurement
./include/measure_defaults.h  # configures server behavior
./external/   # tools for processing and analyzing stack traces
```

### ./external
This project is controlled via the `./external/Makefile` with some extensions in `./external/Antonia.make`
that rely on `../../../Dockerfile`. See the Make-variable `MAKEEXTRA` and the line `include $(MAKEEXTRA)`.

Configure the variables in `./external/Makefile` according to your directory structures and requirements.
If anything is insufficiently documented, [write me an email](mailto:antonia.obersteiner@tu-dresden.de).

The results are written to `./external/data/`, usually in a subdirectory, to differentiate configurations.
The subdirectory ('label') currently has to be explicitly named via `make LABEL=`.

```bash
make LABEL=test data/test/hello.svg
# equivalent to
make LABEL=test MODULE=hello ENDING=svg default
```

Possible endings that can be substituted for `svg` above:

- `traced`: runs the docker, collects the output
    - relies on `Antonia.make`, configure your own `SAMPLE_PATH` in `./external/Makefile`
- `cleaned`: removes control characters from `traced`
	- exported data should be written as hex u64 in the text with `>=<` to mark the lines
	- data has binary format with (usually 1KiB) blocks (simple and not yet very useful XOR redundancy)
- `compressed`: extract binary data from `cleaned`: contiguous, without redundancy blocks
	- still with dictionary compression (in larger blocks, usually 8KiB)
- `btb`: decompressed backtrace buffer binary format as written inside the JDB BTB Kernel implementation
- `interpreted`: human readable version of BTB format
- `folded`: line-for-line stack-traces, input for FlameGraph
- `svg`: output of FlameGraph
- `log`: makes all of the above and does not delete intermediate files

### Selection of the Traced Program and `.cfg` files

The build system with `Antonia.make` assumes that if you use `MODULE=hello`, you have a
`$(L4DIR)/conf/examples/hello-backtraced.cfg` and include it in your L4 `MODULEPATH`.

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
- clean up backtracer block system
- network export? [partially deprecated]
    - Till M nach UDP-Code fragen
    - luna, incl. Beispielprogramm
    - qemu nic-dev-type=e1000 oder so
    - tune-?tab-device verbindet sich in qemu, auf host dann mit bridge

