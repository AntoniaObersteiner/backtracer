
CFLAGS= --max-errors=3 -ggdb
HEADERS=\
	block.h \

SAMPLE=../../../../../docker_log/good_sample
OUTPUT=example.btb

unpack: unpack.c $(HEADERS)

$(SAMPLE).cleaned: $(SAMPLE)
	cat -v $(SAMPLE) > $(SAMPLE).cleaned

.PHONY: run
run: unpack $(SAMPLE).cleaned
	./unpack $(SAMPLE).cleaned $(OUTPUT)
	wc $(OUTPUT)

.PHONY: gdb
gdb: unpack
	gdb -tui --command=test.gdb ./unpack

.PHONY: clean
clean:
	rm -f $(OUTPUT) ./stderr ./stdout ./unpack
