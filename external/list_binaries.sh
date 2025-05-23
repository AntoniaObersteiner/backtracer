path=$1
output_file=$2

# this script is built to be run first with empty first arg 
# and then subsequently with directories to read binaries from
if [ -z "$path" ]; then
	echo "clearing and writing KERNEL to '$output_file'"
	echo "KERNEL: ../../../../build/amd64/fiasco/fiasco.debug" > $output_file
	exit
fi
if [ ! -d $path ]; then
	echo "'$path' is not a valid path from pwd $(pwd)!";
	exit
fi

echo "adding to '$output_file' from '$path'"
for f in $(ls $path); do
	if [ -L $path/$f ]; then
		if [ -f $(readlink $path/$f) ]; then
			echo "rom/$f: $(readllink $path/$f)" >> $output_file
		else
			rebased=$(readlink $path/$f | sed 's+/build+../../../../build+')
			if [ -f $rebased ]; then
				echo "rom/$f: $rebased" >> $output_file
			fi
		fi
	else
		if [ -x $path/$f ]; then
			echo "rom/$f: $path/$f" >> $output_file
		else
			echo "$path/$f not executable"
		fi
	fi
done

