#!/usr/bin/bash
apps="$(ls data/measure*/*.svg | cut -d'/' -f3 | cut -d'.' -f1 | sort -u)"
echo "apps: '$apps'"

for app in $apps; do
	relink_dir="data/app_relink_$app";
	mkdir -p "$relink_dir";
	for measure in $(ls data/measure*/$app.svg | cut -d'/' -f2 | sort -u); do
		echo "ln -fs \"../$measure/$app.svg\" \"$relink_dir/$measure.svg\"";
		ln -fs "../$measure/$app.svg" "$relink_dir/$measure.svg";
	done	
done
