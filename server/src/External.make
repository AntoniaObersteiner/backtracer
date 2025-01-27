SHELL=bash -o pipefail -e

CXX=g++-13

# source directory
S=./src
# include directory
I=../../include
# build directory (not named ./build, consider fiasco and l4's ./__build__ and /build complexities)
O=./objects
# data (intermediate objects and samples
D=./data
BASE_PATH=../../../../..
BUILD_PATH=$(BASE_PATH)/__build__

FLAME_GRAPH:=$(BASE_PATH)/FlameGraph
ELFIO_PATH:=$(BASE_PATH)/ELFIO

CFLAGS:= --max-errors=3 -ggdb -I$I
CXXFLAGS:= --max-errors=3 -ggdb --std=c++20 -I$(ELFIO_PATH) -I$I -MMD -MP
CHEADERS:=$(addprefix $I/,\
	block.h \
)

CXXHEADERS:=$(addprefix $S/,\
	elfi.hpp \
	mmap_file.hpp \
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

SAMPLE_RELPATH=docker_log
SAMPLE_PATH=$(BASE_PATH)/$(SAMPLE_RELPATH)

SAMPLE:=$(SAMPLE_PATH)/$(MODULE).traced
CLEANED:=$D/$(MODULE).cleaned
COMPRESSED:=$D/$(MODULE).compressed
BUFFER:=$D/$(MODULE).btb
INTERPRETED:=$D/$(MODULE).interpreted

BINARY_DIR=$(BASE_PATH)/__build__/amd64/l4/bin/amd64_gen/l4f/.debug
BINARY_LIST=$D/binaries.list

ELFDUMP=ELFIO/examples/elfdump/elfdump

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

$(SAMPLE_PATH)/%.serial:
	 sudo minicom -D /dev/ttyUSB0 -C $@

$(SAMPLE_PATH)/%.traced:
	cd $(BASE_PATH) && sudo                                       \
		OUTPUT=$(subst $(BASE_PATH)/,,$@)                         \
		./start_docker.sh                                         \
		./docker.sh                                               \
		$*-backtraced

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

%.svg: %.folded $(FLAME_GRAPH)/flamegraph.pl
	$(FLAME_GRAPH)/flamegraph.pl $< > $@

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
		$*.svg \
		|& tee $@
	
	# making the .folded file also creates the -0.folded, -1.folded, ...
	# files for all observed cpu ids. we go through these files
	# and create svgs for them
	for f in $$(ls $*-*.folded); do								  \
		make -f External.make $${f/folded/svg};					  \
	done

.PHONY: copy_results
copy_results:
	rsync -avuP data/ ~/mnt/pas/Antonia/STUDIUM/08/GB/data

L4_BINARIES=\
	$(shell find $(BUILD_PATH)/amd64/l4/bin/amd64_gen/l4f/ -maxdepth 1 -type f) \
	$(BUILD_PATH)/amd64/fiasco/fiasco \
	$(BUILD_PATH)/amd64/l4/bin/amd64_gen/plain/bootstrap \

L4_CONFIGS=\
	$(shell find $(BASE_PATH)/l4/conf/examples/ -name '*-backtraced.cfg')

.PHONY: pxe_install
pxe_install:
	make -f External.make pxe_menu
	# copy all needed modules/config to server:boot/
	echo "bins: $(L4_BINARIES)"
	echo "conf: $(L4_CONFIGS)"
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

.PHONY: fiasco_config
fiasco_config:
	cd $(BASE_PATH) && sudo                                       \
		./start_docker.sh                                         \
		make -C /build/amd64/fiasco menuconfig

.PHONY: l4_config
l4_config:
	cd $(BASE_PATH) && sudo                                       \
		./start_docker.sh                                         \
		make -C /build/amd64/l4 menuconfig

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
	echo "b main" > test_interpret.gdb
	echo "run $(BUFFER) $(BUFFER:.btb=.interpreted) > ./stdout 2> ./stderr" >> test_interpret.gdb

	gdb -tui --command=test_interpret.gdb ./interpret

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

$(ELFDUMP):
	cd $(BASE_PATH)/ELFIO/; cmake .
	make -C $(BASE_PATH)/ELFIO/ elfdump

$O/fiasco.elfdump: $(ELFDUMP)
	$(ELFDUMP) __build__/amd64/fiasco/fiasco.debug | tee $@

$O/%.disas:
	export link=$$(readlink $(BINARY_DIR)/$*)             && \
	export file=$(BASE_PATH)$${link/build/__build__}      && \
	echo "file: $$file"                                   && \
	objdump -lSd  "$$file" > $@

.PHONY: clean
clean:
	# note: does not remove .cleaned or .traced files, because tracing takes so long
	rm -f \
		$D/*.btb $D/*.compressed $D/*.interpreted $D/*.folded $D/*.svg \
		./stderr ./stdout \
		./unpack ./interpret \
		$(CXXDEPENDENCIES) $(CXXOBJECTS)
