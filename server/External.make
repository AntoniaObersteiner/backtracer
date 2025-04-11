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
# if "" or "y", rebuild. if "n", don't
DOCKER_BUILD?=

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

SAMPLE_RELPATH?=docker_log
SAMPLE_PATH?=$(BASE_PATH)/$(SAMPLE_RELPATH)

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

.PHONY: default
default: $D/$(LABEL)/$(MODULE).svg

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

%.serial:
	sudo minicom -D /dev/ttyUSB0 -C $@

#%.traced: %.serial
	#cp $< $@

$(SAMPLE_PATH)/%.traced:
	cd $(BASE_PATH) && sudo                          \
		OUTPUT=$(subst $(BASE_PATH)/,,$@)            \
		BUILD=$(DOCKER_BUILD)                        \
		./start_docker.sh                            \
		./docker.sh                                  \
		$*-backtraced

.PHONY: build
build:
	cd $(BASE_PATH) && sudo                          \
		./start_docker.sh                            \
		./docker.sh                                  \
		build

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

%.runnings.svg: %.durations ./tools/durations.py
	./tools/durations.py $<

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

%.cleaned.svg: %.cleaned
	# read in the qsort debug print output and plot the left/right distribution
	python3 $(L4DIR)/pkg/qsort/server/src/plot_steps.py $<

%.pdf: %.svg
	rsvg-convert -f pdf -o $@ $<

%.log:
	# creating the .log is mostly used to make all the intermediates
	# without throwing them away because of make's rules for intermediate file
	make -f External.make $*.traced
	make -f External.make \
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
		$*.runnings.svg \
		|& tee $@
	
	# making the .folded file also creates the -0.folded, -1.folded, ...
	# files for all observed cpu ids. we go through these files
	# and create svgs for them
	# deactivated because we don't to multi-processor currently
	# for f in $$(ls $*-*.folded); do								  \
	# 	make -f External.make $${f/folded/svg};					  \
	# done

.PHONY: rsync_to_pas
rsync_to_pas:
	rsync -avuP data/ ~/mnt/pas/Antonia/STUDIUM/08/GB/data

.PHONY: rsync_to_oz
rsync_to_oz:
	rsync -avuP data oz:Antonia/STUDIUM/os/fl4mer/

L4_BINARIES=\
	$(shell find $(L4RE_BUILD_PATH)/bin/amd64_gen/l4f/ -maxdepth 1 -type f) \
	$(FIASCO_BUILD_PATH)/fiasco \
	$(L4RE_BUILD_PATH)/bin/amd64_gen/plain/bootstrap \

L4_CONFIGS=\
	$(shell find $(BASE_PATH)/l4/conf/examples/ -name '*-backtraced.cfg')

.PHONY: pxe_install
pxe_install:
	make -f External.make pxe_menu
	# copy all needed modules/config to server:boot/
	rsync -avuP \
		$(L4_BINARIES) \
		$(L4_CONFIGS) \
		menu.lst \
		erwin:boot/

.PHONY: pxe_menu
pxe_menu:
	./tools/generate_pxe_menu.py \
		--output-filename menu.lst \
		--bins $(L4_BINARIES) \
		--conf $(L4_CONFIGS)

.PHONY: fiasco_swap_kconfig
fiasco_swap_kconfig:
	# use rsync so no unnecessary copying triggers full recopmilation
	sudo rsync -avuP $(FIASCO_BUILD_PATH)/$(KCONFIG).out $(FIASCO_BUILD_PATH)/$(KCONFIG_BASE).out
	sudo rsync -avuP $(FIASCO_BUILD_PATH)/$(KCONFIG).h   $(FIASCO_BUILD_PATH)/$(KCONFIG_BASE).h

.PHONY: fiasco_config
fiasco_config:
	cd $(BASE_PATH) && sudo                                       \
		./start_docker.sh                                         \
		make -C /build/amd64/fiasco menuconfig
	sudo cp $(FIASCO_BUILD_PATH)/$(KCONFIG_BASE).out $(FIASCO_BUILD_PATH)/$(KCONFIG).out
	sudo cp $(FIASCO_BUILD_PATH)/$(KCONFIG_BASE).h   $(FIASCO_BUILD_PATH)/$(KCONFIG).h

.PHONY: l4_config
l4_config:
	cd $(BASE_PATH) && sudo                                       \
		./start_docker.sh                                         \
		make -C /build/amd64/l4 menuconfig

.PHONY: docker_bash
docker_bash:
	cd $(BASE_PATH) && sudo                                       \
		./start_docker.sh                                         \
		bash

.PHONY: sample_length
sample_length:
	./length_of_data.sh $(CLEANED)

.PHONY: gdb
gdb_unpack: unpack $(CLEANED)
	echo "b main" > test_unpack.gdb
	echo "run $(CLEANED) $(COMPRESSED) > ./stdout 2> ./stderr" >> test_unpack.gdb

	gdb -tui --command=test_unpack.gdb ./unpack

.PHONY: gdb_interpret
gdb_interpret: interpret $(BUFFER)
	echo "b main" > test_$(INTERPRET_TEST_MODE).gdb
	echo "run $(BUFFER) $(BUFFER:.btb=.$(INTERPRET_TEST_MODE)) > ./stdout 2> ./stderr" \
		>> test_$(INTERPRET_TEST_MODE).gdb

	gdb -tui --command=test_$(INTERPRET_TEST_MODE).gdb ./interpret

.PHONY: gdb_folded
gdb_folded:
	make -f External.make INTERPRET_TEST_MODE=folded

.PHONY: gdb_histgram
gdb_histgram:
	make -f External.make INTERPRET_TEST_MODE=histogram

.PHONY: gdb_compress
gdb_compress: test_compress
	echo "b main" > test_compress.gdb
	echo "b compress.cpp:70" > test_compress.gdb
	echo "run > ./stdout 2> ./stderr" >> test_compress.gdb

	gdb -tui --command=test_compress.gdb ./test_compress

.PHONY: gdb_decompress
gdb_decompress: test_compress $(COMPRESSED)
	echo "b main" > test_decompress.gdb
	echo "run $(COMPRESSED) $(BUFFER) > ./stdout 2> ./stderr" >> test_decompress.gdb

	gdb -tui --command=test_decompress.gdb ./decompress

$(ELFIO_PATH)/$(ELFDUMP):
	cd $(ELFIO_PATH); cmake .
	make -C $(ELFIO_PATH)/ elfdump

$O/fiasco.elfdump: $(ELFIO_PATH)/$(ELFDUMP)
	target=$$(pwd)/$@; \
	$(ELFIO_PATH)/$(ELFDUMP) $(FIASCO_BUILD_PATH)/fiasco.debug \
		| tee $$target

$O/%.disas:
	objdump -lSd $(BASE_PATH)$$(readlink $(BINARY_DIR)/$*) > $@

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
