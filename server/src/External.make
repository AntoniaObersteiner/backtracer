
CFLAGS= --max-errors=3 -ggdb
CXXFLAGS= --max-errors=3 -ggdb --std=c++20
HEADERS=\
	block.h \

SAMPLE=../../../../../docker_log/good_sample
SAMPLE=../../../../../docker_log/full_sample
SAMPLE=../../../../../docker_log/long_sample
OUTPUT=example.btb
BINARY_DIR=../../../../../__build__/amd64/l4/bin/amd64_gen/l4f/

unpack: unpack.c $(HEADERS)
interpret: interpret.cpp
elfi: elfi.hpp

binaries.list: list_binaries.sh $(BINARY_DIR)
	./list_binaries.sh $(BINARY_DIR)

$(SAMPLE).cleaned: $(SAMPLE)
	cat -v $(SAMPLE) > $(SAMPLE).cleaned

.PHONY: run
run: unpack $(SAMPLE).cleaned interpret
	./unpack $(SAMPLE).cleaned $(OUTPUT)
	./interpret $(OUTPUT)

.PHONY: gdb
gdb: unpack
	gdb -tui --command=test.gdb ./unpack

.PHONY: gdb_interpret
gdb_interpret: interpret
	gdb -tui --command=test_interpret.gdb ./interpret

.PHONY: clean
clean:
	rm -f $(OUTPUT) ./stderr ./stdout ./unpack ./interpret
