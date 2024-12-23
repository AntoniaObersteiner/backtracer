SHELL=bash

CXX=g++-13

BASE_PATH=../../../../..

CFLAGS:= --max-errors=3 -ggdb
CXXFLAGS:= --max-errors=3 -ggdb --std=c++20 -I$(BASE_PATH)/ELFIO
HEADERS:=\
	block.h \

# keep the line above free
CXXHEADERS:=\
	elfi.hpp \

# keep the line above free

SAMPLE_RELPATH=docker_log
SAMPLE_PATH=$(BASE_PATH)/$(SAMPLE_RELPATH)
SAMPLE_NAME?=sample
MODULE?=qsort-backtraced

SAMPLE:=$(SAMPLE_PATH)/$(SAMPLE_NAME)
CLEANED:=$(SAMPLE_PATH)/$(SAMPLE_NAME).cleaned
BUFFER:=$(SAMPLE_NAME).btb
OUTPUT:=$(SAMPLE_NAME).traces

BINARY_DIR=$(BASE_PATH)/__build__/amd64/l4/bin/amd64_gen/l4f/.debug
BINARY_LIST=binaries.list

ELFDUMP=ELFIO/examples/elfdump/elfdump

.PHONY: all
all: get_sample $(OUTPUT)

unpack: unpack.c $(HEADERS)
interpret: interpret.cpp $(CXXHEADERS)

$(BINARY_LIST): list_binaries.sh $(BINARY_DIR)
	./list_binaries.sh $(BINARY_DIR) $(BINARY_LIST)

# replaces the control characters by printable representations (e.g. ^M)
%.cleaned: %
	cat -v $< > $@

%.btb: $(SAMPLE_PATH)/%.cleaned unpack
	# unpack the printed hex to the backtrace buffer binary format
	./unpack $< $@

%.traces: %.btb interpret $(BINARY_LIST)
	# interpret the backtrace buffer binary format and print to $@ file
	./interpret $< |& tee $@

.PHONY: get_sample
get_sample:
	cd $(BASE_PATH); sudo OUTPUT=$(SAMPLE_RELPATH)/$(SAMPLE_NAME) ./start_docker.sh ./docker.sh $(MODULE)

.PHONY: run
run: $(SAMPLE).traces

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
	echo "run $(BUFFER) > ./stdout 2> ./stderr" >> test_interpret.gdb

	gdb -tui --command=test_interpret.gdb ./interpret

$(ELFDUMP):
	cd $(BASE_PATH)/ELFIO/; cmake .
	make -C $(BASE_PATH)/ELFIO/ elfdump

fiasco.elfdump: $(ELFDUMP)
	$(ELFDUMP) __build__/amd64/fiasco/fiasco.debug | tee $@

.PHONY: clean
clean:
	rm -f *.btb ./stderr ./stdout ./unpack ./interpret *.traces
