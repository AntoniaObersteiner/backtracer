#!/usr/bin/bash

### RUN THIS IN AN EMPTY DIRECTORY TO INSTALL FIASCO AND FL4MER,
# otherwise, please read and use what you need

# get the current state of all necessary repositories
# the first commit belonging to fl4mer is tagged with tag fl4mer in these,
# in case you want to cherry-pick
git clone git-i1@git.l4re.org:fossil/fiasco.git       fiasco              --branch fl4mer
git clone git-i1@git.l4re.org:fossil/mk.git           l4                  --branch fl4mer
git clone git-i1@git.l4re.org:fossil/bootstrap.git    l4/pkg/bootstrap    --branch fl4mer
git clone git-i1@git.l4re.org:fossil/drivers-frst.git l4/pkg/drivers-frst --branch fl4mer
git clone https://github.com/kernkonzept/zlib.git     l4/pkg/zlib         --branch master
git clone https://github.com/kernkonzept/gnu-efi.git  l4/pkg/gnu-efi      --branch master
# the module with all the user side backtracer code
git clone git@github.com:AntoniaObersteiner/backtracer.git l4/pkg/backtracer --branch master 

# get examples for usage to track during a specific application's runtime
# these should work with the example comfigs in mk.git in /l4.
git clone git@github.com:AntoniaObersteiner/qsort.git                                   l4/pkg/qsort
git clone git@gitlab.hrz.tu-chemnitz.de:anob943c-at-tu-dresden.de/hello-backtraced.git  l4/pkg/hello
git clone git@gitlab.hrz.tu-chemnitz.de:anob943c-at-tu-dresden.de/stress-backtraced.git l4/pkg/stress


