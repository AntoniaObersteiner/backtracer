SHELL=bash -o pipefail -e

CXX=g++-13

# source directory
S=./src
# include directory
I=../include
# build directory (not named ./build, consider fiasco and l4's ./build complexities)
O=./objects
# data (intermediate objects and samples
D=./data
ARCH?=amd64
PKGDIR=..
L4DIR=$(PKGDIR)/../..
BASE_PATH?=../../../..
BUILD_PATH?=$(BASE_PATH)/build
FIASCO_BUILD_PATH?=$(BUILD_PATH)/$(ARCH)/fiasco
L4RE_BUILD_PATH?=$(BUILD_PATH)/$(ARCH)/l4
MAKEFLAGS?=
MAKEEXTRA?=Antonia.make
include $(MAKEEXTRA)

FLAME_GRAPH:=$(PKGDIR)/FlameGraph
ELFIO_PATH:=$(PKGDIR)/ELFIO
ELFDUMP=examples/elfdump/elfdump
FLAME_GRAPH_OPTIONS:=\
	--subtitle "L4Re/Fiasco Backtracer" \
	--width 800 \
	--minwidth 8 \

	#--fonttype "TeX Gyre Schola" \

CFLAGS:= --max-errors=3 -ggdb -I$I
CXXFLAGS:= --max-errors=3 -ggdb --std=c++20 -I$(ELFIO_PATH) -I$I -MMD -MP
CHEADERS:=$(addprefix $I/,\
	block.h \
)

CXXHEADERS:=$(addprefix $S/,\
	elfi.hpp \
	mmap_file.hpp \
	rethrow_error.hpp \
	EntryArray.hpp \
	EntryDescriptor.hpp \
	Entry.hpp \
	Mapping.hpp \
	BinariesList.hpp \
	SymbolTable.hpp \
	Range.hpp \
)

CXXOBJECTS:=$(addprefix $O/,\
	elfi.o \
	interpret.o \
	mmap_file.o \
	EntryArray.o \
	EntryDescriptor.o \
	Entry.o \
	Mapping.o \
	BinariesList.o \
	SymbolTable.o \
)

CXXDEPENDENCIES := $(addprefix $O/,$(CXXOBJECTS:.o=.d))

MODULE?=qsort
# directory in data/ to save results to, used to differentiate configurations
LABEL?=any

# set here, in the environment, in another Makefile, ...
SAMPLE_PATH?=

SAMPLE:=$(SAMPLE_PATH)/$(MODULE).traced
CLEANED:=$D/$(MODULE).cleaned
COMPRESSED:=$D/$(MODULE).compressed
BUFFER:=$D/$(MODULE).btb
INTERPRETED:=$D/$(MODULE).interpreted

BINARY_DIR?=$(L4RE_BUILD_PATH)/bin/amd64_gen/l4f/.debug
BINARY_LIST?=$D/binaries.list

# these are filenames of kernel configs (without .out/.h)
# $(FIASCO_BUILD_PATH) is always prepended
KCONFIG_BASE=globalconfig
KCONFIG=fullkbt

# with what file output to test ./interpret,
# see gdb_interpret and similar below
INTERPRET_TEST_MODE?=interpreted

END?=svg

.PHONY: default
default: $D/$(LABEL)/$(MODULE).$(END)

unpack: $S/unpack.c $(CHEADERS)
	$(CC) -o $@ $< $(CFLAGS)
interpret: $(CXXOBJECTS) $(CXXHEADERS)
	$(CXX) -o $@ $(CXXOBJECTS) $(CXXFLAGS)
test_compress: $O/test_compress.o $O/compress.o $S/compress.hpp
	$(CXX) -o $@ $(filter %.o,$+)
decompress: $O/decompress.o $O/compress.o $O/mmap_file.o $S/compress.hpp
	$(CXX) -o $@ $(filter %.o,$+)

.NOTINTERMEDIATE:

$(SAMPLE_RELPATH)/%.traced:
$D/%.traced:
$D/%.cleaned:
$D/%.compressed:
$D/%.btb:
$D/%.folded:
$D/%.interpreted:

$O/%.o: $S/%.cpp
	$(CXX) $< -c -o $@ $(CXXFLAGS)

$(BINARY_LIST): $(BINARY_DIR) list_binaries.sh
	./list_binaries.sh $< $@

$D/%.traced: $(SAMPLE_PATH)/%.traced
	mkdir -p $(@D)
	cp $< $@

$D/$(LABEL)/%.traced: $(SAMPLE_PATH)/%.traced
	mkdir -p $(@D)
	cp $< $@

# replaces the control characters by printable representations (e.g. ^M)
%.cleaned: %.traced
	cat -v $< | sed 's/\^\[\[[0-9]*m//g' | sed 's/\^M//g' > $@

%.compressed: %.cleaned unpack
	# unpack the printed hex to the compressed btb format
	./unpack $< $@

%.btb: %.compressed decompress
	# decompress to the backtrace buffer binary format
	./decompress $< $@

%.interpreted: %.btb interpret $(BINARY_LIST)
	# interpret the backtrace buffer binary format and write to $@ file, log to $@.log
	./interpret $< $@

%.folded: %.btb interpret $(BINARY_LIST)
	./interpret $< $@

%.histogram: %.btb interpret $(BINARY_LIST)
	./interpret $< $@

%.durations: %.btb interpret $(BINARY_LIST)
	./interpret $< $@

%.histogram.svg: %.histogram ./tools/hist_plot.py
	./tools/hist_plot.py $<

%.durations.svg: %.durations ./tools/hist_plot.py
	./tools/hist_plot.py $<

# temporarily broken, see comment in tools/durations.py
# %.runnings.svg: %.durations ./tools/durations.py
# 	./tools/durations.py $<

%.svg: %.folded $(FLAME_GRAPH)/flamegraph.pl
	$(FLAME_GRAPH)/flamegraph.pl \
		$(FLAME_GRAPH_OPTIONS) \
		--title "Flame Graph $(*F)" \
		$< > $@

%.fine.svg: %.folded $(FLAME_GRAPH)/flamegraph.pl
	$(FLAME_GRAPH)/flamegraph.pl \
		--subtitle "L4Re/Fiasco Backtracer" \
		--title "Flame Graph $(*F)" \
		--minwidth 1 \
		$< > $@

%.narrow.svg: %.folded $(FLAME_GRAPH)/flamegraph.pl
	$(FLAME_GRAPH)/flamegraph.pl \
		--subtitle "L4Re/Fiasco Backtracer" \
		--title "Flame Graph $(*F)" \
		--minwidth 1 \
		--width 400 \
		$< > $@

%.ultrafine.svg: %.folded $(FLAME_GRAPH)/flamegraph.pl
	$(FLAME_GRAPH)/flamegraph.pl \
		--subtitle "L4Re/Fiasco Backtracer" \
		--title "Flame Graph $(*F)" \
		--minwidth 0 \
		$< > $@

%.pdf: %.svg
	rsvg-convert -f pdf -o $@ $<

%.log:
	# creating the .log is mostly used to make all the intermediates
	# without throwing them away because of make's rules for intermediate file
	make $(MAKEFLAGS) $*.traced
	make $(MAKEFLAGS) \
		$*.cleaned \
		$*.compressed \
		$*.btb \
		$*.interpreted \
		$*.folded \
		$*.histogram \
		$*.durations \
		$*.svg \
		$*.fine.svg \
		$*.ultrafine.svg \
		$*.pdf \
		$*.histogram.svg \
		$*.histogram.pdf \
		$*.durations.svg \
		$*.durations.pdf \
		|& tee $@
	
		# $*.runnings.svg \

	# making the .folded file also creates the -0.folded, -1.folded, ...
	# files for all observed cpu ids. we go through these files
	# and create svgs for them
	# deactivated because we don't to multi-processor currently
	# for f in $$(ls $*-*.folded); do								  \
	# 	make $(MAKEFLAGS) $${f/folded/svg};					  \
	# done

L4_BINARIES=\
	$(shell find $(L4RE_BUILD_PATH)/bin/amd64_gen/l4f/ -maxdepth 1 -type f) \
	$(FIASCO_BUILD_PATH)/fiasco \
	$(L4RE_BUILD_PATH)/bin/amd64_gen/plain/bootstrap \

L4_CONFIGS=\
	$(shell find $(BASE_PATH)/l4/conf/examples/ -name '*-backtraced.cfg')

.PHONY: clear
clear:
	rm -rf data/$(LABEL)

.PHONY: clean
clear_data:
	# note: does not remove .cleaned or .traced files, because tracing takes so long
	rm -f \
		$D/*.btb $D/*.compressed $D/*.interpreted $D/*.folded $D/*.svg \
		./stderr ./stdout \
		./unpack ./interpret \
		$(CXXDEPENDENCIES) $(CXXOBJECTS)
