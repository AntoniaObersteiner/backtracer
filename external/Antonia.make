# if "" or "y", rebuild. if "n", don't
DOCKER_BUILD?=
MAKEFLAGS?=

SAMPLE_RELPATH?=docker_log
SAMPLE_PATH?=$(BASE_PATH)/$(SAMPLE_RELPATH)

# these are filenames of kernel configs (without .out/.h)
# $(FIASCO_BUILD_PATH) is always prepended
KCONFIG_BASE=globalconfig
KCONFIG=fullkbt

# with what file output to test ./interpret,
# see gdb_interpret and similar below
INTERPRET_TEST_MODE?=interpreted

# things to copy into pxe-boot directories
L4_BINARIES=\
	$(shell find $(L4RE_BUILD_PATH)/bin/amd64_gen/l4f/ -maxdepth 1 -type f) \
	$(FIASCO_BUILD_PATH)/fiasco \
	$(L4RE_BUILD_PATH)/bin/amd64_gen/plain/bootstrap \

L4_CONFIGS=\
	$(shell find $(BASE_PATH)/l4/conf/examples/ -name '*-backtraced.cfg')

# this first rule of the this file redirect to the first rule of the main Makefile
.PHONY: _extra_default
_extra_default: default

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

%.cleaned.svg: %.cleaned
	# read in the qsort debug print output and plot the left/right distribution
	python3 $(L4DIR)/pkg/qsort/server/src/plot_steps.py $<

.PHONY: rsync_to_pas
rsync_to_pas:
	rsync -avuP data/ ~/mnt/pas/Antonia/STUDIUM/08/GB/data

.PHONY: rsync_to_oz
rsync_to_oz:
	rsync -avuP data oz:Antonia/STUDIUM/os/fl4mer/

.PHONY: pxe_install
pxe_install:
	make $(MAKEFLAGS) pxe_menu
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
	make $(MAKEFLAGS) INTERPRET_TEST_MODE=folded

.PHONY: gdb_histgram
gdb_histgram:
	make $(MAKEFLAGS) INTERPRET_TEST_MODE=histogram

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
	if [ -f $(BASE_PATH)$$(readlink $(BINARY_DIR)/$*) ]; then \
		objdump -lSd $(BASE_PATH)$$(readlink $(BINARY_DIR)/$*) > $@; \
	else \
		objdump -lSd $(BASE_PATH)$$(readlink $(LIBRARY_DIR)/$*) > $@; \
	fi
