path=$1
output_file=$2
if [ ! -d $path ]; then
	echo "'$path' is not a valid path from pwd $(pwd)!";
	exit
fi

echo "clearing and writing '$output_file'"
echo "" > $output_file

for f in $(ls $path); do
	if [ -x $path/$f ]; then
		echo "$f: $path/$f" >> $output_file
	else
		echo "$path/$f not executable"
	fi
done
