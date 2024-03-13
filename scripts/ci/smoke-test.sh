#!/usr/bin/env bash

if ! test -x "$1/labwc"; then
	echo "$1/labwc not found"
	exit 1
fi

args=(
	"$1/labwc"
	-C scripts/ci
	-d
)

export XDG_RUNTIME_DIR=$(mktemp -d)
export WLR_BACKENDS=headless

echo "Starting ${args[@]}"
output=$("${args[@]}" 2>&1)
ret=$?

if test $ret -ge 128; then
	# Not using -Db_sanitize=address,undefined
	# because it slows down the usual execution
	# way too much and spams pages over pages
	# of irrelevant memory-still-in-use logs
	# for external libraries.
	#
	# Not using coredumps either because they
	# are a pain to setup on GH actions and
	# just running labwc again is a lot faster
	# anyway.

	echo
	echo "labwc crashed, restarting under gdb"
	echo
	gdb --batch                       \
		-ex run                   \
		-ex 'bt full'             \
		-ex 'echo \n'             \
		-ex 'echo "Local vars:\n' \
		-ex 'info locals'         \
		-ex 'echo \n'             \
		-ex 'set listsize 50'     \
		-ex list                  \
		--args "${args[@]}"
else
	echo "$output"
fi

echo "labwc terminated with return code $ret"
exit $ret
