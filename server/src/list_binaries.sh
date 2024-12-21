path=$1
output_file=$2
if [ ! -d $path ]; then
	echo "'$path' is not a valid path from pwd $(pwd)!";
	exit
fi

echo "clearing and writing '$output_file'"
echo "" > $output_file

for f in $(ls $path); do
	if [ -L $path/$f ]; then
		if [ -f $(readlink $path/$f) ]; then
			echo "$f: $(readllink $path/$f)" >> $output_file
		else
			rebased=$(readlink $path/$f | sed 's+/build+../../../../../__build__+')
			if [ -f $rebased ]; then
				echo "$f: $rebased" >> $output_file
			fi
		fi
	else
		if [ -x $path/$f ]; then
			echo "$f: $path/$f" >> $output_file
		else
			echo "$path/$f not executable"
		fi
	fi
done

echo "KERNEL: __build__/amd64/fiasco/fiasco.dbg" >> $output_file
