#!/usr/bin/bash

binaries=$(find build/amd64/l4/bin/amd64_gen/l4f/ -maxdepth 1 -type f)
binaries="$binaries build/amd64/fiasco/fiasco"
binaries="$binaries build/amd64/l4/bin/amd64_gen/plain/bootstrap"

configs=$(find l4/conf/examples/ -name '*-backtraced.cfg')

echo "bins: $binaries"
echo "conf: $configs"

# exit

rsync -avuP $binaries erwin:boot/
rsync -avuP $configs  erwin:boot/
