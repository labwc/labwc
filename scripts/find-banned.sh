#!/bin/sh

banned="malloc,g_strcmp0,sprintf,vsprintf,strcpy,strncpy,strcat,strncat,atof"

echo "Searching for banned functions"
find src/ include/ \( -name "*.c" -o -name "*.h" \) -type f \
	| ./scripts/helper/find-idents --tokens=$banned -
