SHELL=bash

CFLAGS= --max-errors=3 -ggdb
CXXFLAGS= --max-errors=3 -ggdb --std=c++20 -I../../../../../ELFIO
HEADERS=\
	block.h \

CXXHEADERS=\
	elfi.hpp \

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

$(SAMPLE).cleaned: $(SAMPLE)
	cat -v $(SAMPLE) > $(SAMPLE).cleaned

.PHONY: run
run: unpack $(SAMPLE).cleaned interpret $(BINARY_LIST)
	./unpack $(SAMPLE).cleaned $(BUFFER)
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
	rm -f $(BUFFER) ./stderr ./stdout ./unpack ./interpret $(OUTPUT)
