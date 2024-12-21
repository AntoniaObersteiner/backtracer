SHELL=bash

CXX=g++-13

CFLAGS= --max-errors=3 -ggdb
CXXFLAGS= --max-errors=3 -ggdb --std=c++20 -I../../../../../ELFIO
HEADERS=\
	block.h \

CXXHEADERS=\
	elfi.hpp \

SAMPLE_PATH=../../../../../docker_log
SAMPLE=../../../../../docker_log/good_sample
SAMPLE=../../../../../docker_log/full_sample
SAMPLE=../../../../../docker_log/long_sample
SAMPLE=../../../../../docker_log/fresh_giant
BUFFER=example.btb
OUTPUT=example.traces
BINARY_DIR=../../../../../__build__/amd64/l4/bin/amd64_gen/l4f/.debug
BINARY_LIST=binaries.list

unpack: unpack.c $(HEADERS)
interpret: interpret.cpp $(CXXHEADERS)

$(BINARY_LIST): list_binaries.sh $(BINARY_DIR)
	./list_binaries.sh $(BINARY_DIR) $(BINARY_LIST)

%.cleaned: %
	cat -v $< > $@

%.btb: $(SAMPLE_PATH)/%.cleaned unpack
	./unpack $< $@

%.traces: %.btb interpret
	./interpret $< |& tee $@

.PHONY: run
run: ./interpret $(BUFFER)
	./interpret $(BUFFER) |& tee $(OUTPUT)

.PHONY: sample_length
sample_length:
	./length_of_data.sh $(SAMPLE).cleaned

.PHONY: gdb
gdb: unpack
	gdb -tui --command=test.gdb ./unpack

.PHONY: gdb_interpret
gdb_interpret: interpret
	gdb -tui --command=test_interpret.gdb ./interpret

.PHONY: clean
clean:
	rm -f *.btb ./stderr ./stdout ./unpack ./interpret *.traces
