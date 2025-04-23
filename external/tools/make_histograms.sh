
files=$(find data/ -name stress_malloc.interpreted);
files=$(for f in $files; do
	echo $f "$(grep 'entry_type.*:.*1' $f | wc --lines)";
done | awk '$2 > 300 {print $1}');
for f in $(echo $files | cut -d'.' -f1); do
	./tools/hist_plot.py $f.durations
done
