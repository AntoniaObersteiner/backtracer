#!/usr/bin/bash

### RUN THIS IN AN EMPTY DIRECTORY TO INSTALL FIASCO AND FL4MER,
# otherwise, please read and use what you need
# also, see the README

# get the current state of all necessary repositories
# the first commit belonging to fl4mer is tagged with tag fl4mer in these,
# in case you want to cherry-pick
git clone git-i1@git.l4re.org:fossil/fiasco.git       fiasco              --branch fl4mer
git clone git-i1@git.l4re.org:fossil/mk.git           l4                  --branch fl4mer
git clone git-i1@git.l4re.org:fossil/l4re-core.git    l4/pkg/l4re-core    --branch fl4mer
git clone git-i1@git.l4re.org:fossil/bootstrap.git    l4/pkg/bootstrap    --branch fl4mer
git clone git-i1@git.l4re.org:fossil/drivers-frst.git l4/pkg/drivers-frst --branch fl4mer
git clone https://github.com/kernkonzept/zlib.git     l4/pkg/zlib         --branch master
git clone https://github.com/kernkonzept/gnu-efi.git  l4/pkg/gnu-efi      --branch master
# dependency mismatch :/
echo "provides: libefi" >> l4/pkg/gnu-efi/Control
# << EOF
#required: stdlibs
#provides: libefi
#maintainer: matthias.lange@kernkonzept.com
#EOF

# the module with all the user and external backtracer code, including ELFIO and FlameGraph
git clone git@github.com:AntoniaObersteiner/backtracer.git l4/pkg/backtracer --branch master --recursive

# get examples for usage to track during a specific application's runtime
# these should work with the example comfigs in mk.git in /l4/conf/example.
git clone git@github.com:AntoniaObersteiner/qsort.git                                   l4/pkg/qsort
git clone git@gitlab.hrz.tu-chemnitz.de:anob943c-at-tu-dresden.de/hello-backtraced.git  l4/pkg/hello
#git clone git@gitlab.hrz.tu-chemnitz.de:anob943c-at-tu-dresden.de/stress-backtraced.git l4/pkg/stress


make -C l4     B=../build/amd64/l4
make -C fiasco B=../build/amd64/fiasco
# don't omit frame pointer. so, default seems fine
make -C build/amd64/l4     menuconfig
# no performance configuration!
# *don't* compile without frame pointer.
# allow tracing user and kernel stacks.
make -C build/amd64/fiasco menuconfig
make -C build/amd64/l4     -j 8
make -C build/amd64/fiasco -j 8

make -C build/amd64/l4 E=hello-backtraced qemu \
	QEMU_OPTIONS="-nographic -m 512 -enable-kvm -cpu host -M pc-i440fx-7.2 -smp cpus=1" \
	MODULE_SEARCH_PATH="build/amd64/fiasco:l4/conf/examples" \
	>> hello.traced

make -C l4/pkg/backtracer/external \
	EXTRAMAKE= \
	SAMPLE_PATH=../../../../ \
	LABEL=test \
	MODULE=hello \
	ENDING=log

