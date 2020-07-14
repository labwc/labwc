#!/bin/sh
#
# List available openbox themes
#

printf '%b\n' "#icons\ttheme name"
printf '%b\n' "-------------------"

for d in $HOME/.themes/* /usr/share/themes/*; do
	if [ -d "$d/openbox-3" ]; then
		icon_count=0
		for f in $d/openbox-3/*; do
			case $f in
				*xbm) : $(( icon_count++ )) ;;
			esac
		done
		printf '%b\n' "$icon_count\t$(basename $d)"
	fi
done
