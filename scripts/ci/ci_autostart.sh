#!/usr/bin/env bash

if test -z "$LABWC_PID"; then
	echo "LABWC_PID not set" >&2
	exit 1
fi

echo "Running with pid $LABWC_PID"

# Could add runtime tests here

echo "killing labwc"
kill -s TERM $LABWC_PID
