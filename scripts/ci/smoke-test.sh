#!/usr/bin/env bash

: ${LABWC_RUNS:=1}

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

gdb_run() {
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

	gdb --batch                       \
		--return-child-result     \
		-ex run                   \
		-ex 'bt full'             \
		-ex 'echo \n'             \
		-ex 'echo "Local vars:\n' \
		-ex 'info locals'         \
		-ex 'echo \n'             \
		-ex 'set listsize 50'     \
		-ex list                  \
		--args "${args[@]}"
	return $?
}
echo "running with $LABWC_RUNS runs"
ret=0
for((i=1; i<=LABWC_RUNS; i++)); do
	printf "Starting run %2s\n" $i
	output=$(gdb_run 2>&1)
	ret=$?
	if test $ret -ne 0; then
		echo "Crash encountered:"
		echo "------------------"
		echo "$output"
		break
	fi
done

echo "labwc terminated with return code $ret"
exit $ret
