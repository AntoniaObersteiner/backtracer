SHELL=bash -o pipefail -e

CXX=g++-13

BASE_PATH=../../../../..

FLAME_GRAPH:=$(BASE_PATH)/FlameGraph
ELFIO_PATH:=$(BASE_PATH)/ELFIO

CFLAGS:= --max-errors=3 -ggdb
CXXFLAGS:= --max-errors=3 -ggdb --std=c++20 -I$(ELFIO_PATH) -MMD -MP
HEADERS:=\
	block.h \

# keep the line above free
CXXHEADERS:=\
	elfi.hpp \
	EntryArray.hpp \
	EntryDescriptor.hpp \
	Entry.hpp \
	Mapping.hpp \
	BinariesList.hpp \
	SymbolTable.hpp \
	Range.hpp \

# keep the line above free
CXXOBJECTS:=\
	elfi.o \
	interpret.o \
	EntryArray.o \
	EntryDescriptor.o \
	Entry.o \
	Mapping.o \
	BinariesList.o \
	SymbolTable.o \

CXXDEPENDENCIES := $(CXXOBJECTS:.o=.d)

MODULE?=qsort

SAMPLE_RELPATH=docker_log
SAMPLE_PATH=$(BASE_PATH)/$(SAMPLE_RELPATH)
SAMPLE_NAME?=$(MODULE)

SAMPLE:=$(SAMPLE_PATH)/$(SAMPLE_NAME)
CLEANED:=$(SAMPLE_PATH)/$(SAMPLE_NAME).cleaned
BUFFER:=$(SAMPLE_NAME).btb
OUTPUT:=$(SAMPLE_NAME).interpreted

BINARY_DIR=$(BASE_PATH)/__build__/amd64/l4/bin/amd64_gen/l4f/.debug
BINARY_LIST=binaries.list

ELFDUMP=ELFIO/examples/elfdump/elfdump

.PHONY: default
default: qsort.svg

unpack: unpack.c $(HEADERS)
interpret: $(CXXOBJECTS) $(CXXHEADERS)
	$(CXX) -o $@ $(CXXOBJECTS) $(CXXFLAGS)

%.o: %.cpp %.hpp
	$(CXX) $< -c -o $@ $(CXXFLAGS)

$(BINARY_LIST): list_binaries.sh $(BINARY_DIR)
	./list_binaries.sh $(BINARY_DIR) $(BINARY_LIST)

$(SAMPLE_PATH)/%.traced:
	cd $(BASE_PATH) && sudo                                       \
		OUTPUT=$(subst $(BASE_PATH)/,,$@)                         \
		./start_docker.sh                                         \
		./docker.sh                                               \
		$(subst .traced,,$(subst $(SAMPLE_PATH)/,,$@))-backtraced

# replaces the control characters by printable representations (e.g. ^M)
$(SAMPLE_PATH)/%.cleaned: $(SAMPLE_PATH)/%.traced
	cat -v $< > $@

%.btb: $(SAMPLE_PATH)/%.cleaned unpack
	# unpack the printed hex to the backtrace buffer binary format
	./unpack $< $@

%.interpreted: %.btb interpret $(BINARY_LIST)
	# interpret the backtrace buffer binary format and write to $@ file, log to $@.log
	./interpret $< $@ |& tee $@.log

%.folded: %.btb interpret $(BINARY_LIST)
	./interpret $< $@ |& tee $@.log

%.svg: %.folded $(FLAME_GRAPH)/flamegraph.pl
	$(FLAME_GRAPH)/flamegraph.pl $< > $@

.PHONY: run
run: $(SAMPLE).interpreted

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
gdb_unpack: unpack $(SAMPLE)
	echo "b main" > test_unpack.gdb
	echo "run $(SAMPLE) $(BUFFER) > ./stdout 2> ./stderr" >> test_unpack.gdb

	gdb -tui --command=test_unpack.gdb ./unpack

.PHONY: gdb_interpret
gdb_interpret: interpret $(BUFFER)
	echo "b main" > test_interpret.gdb
	echo "b elfi.hpp:110" > test_interpret.gdb
	echo "run $(BUFFER) > ./stdout 2> ./stderr" >> test_interpret.gdb

	gdb -tui --command=test_interpret.gdb ./interpret

$(ELFDUMP):
	cd $(BASE_PATH)/ELFIO/; cmake .
	make -C $(BASE_PATH)/ELFIO/ elfdump

fiasco.elfdump: $(ELFDUMP)
	$(ELFDUMP) __build__/amd64/fiasco/fiasco.debug | tee $@

.PHONY: clean
clean:
	rm -f \
		*.btb *.interpreted *.folded *.svg \
		./stderr ./stdout \
		./unpack ./interpret \
		$(CXXDEPENDENCIES) $(CXXOBJECTS)
