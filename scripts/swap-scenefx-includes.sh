#!/bin/sh
# Swap #include <wlr/types/wlr_scene.h> for the SceneFX equivalent.
#
# labwc-blur links against SceneFX instead of the wlroots scene library
# (libscenefx re-exports the wlr_scene_* symbols, see the meson.build
# comment about link order), so every file using the scene API must
# include the SceneFX header.  Upstream may add new .c files after the
# patch stack was written; run this script after rebasing onto a new
# upstream tag to catch those files.
#
# Idempotent: already-swapped files are left untouched.

set -eu

cd "$(git rev-parse --show-toplevel)"

old='#include <wlr/types/wlr_scene.h>'
new='#include <scenefx/types/wlr_scene.h>'

files=$(git grep -l -F "$old" -- . || true)
if [ -z "$files" ]; then
	echo "swap-scenefx-includes: nothing to do"
	exit 0
fi

echo "$files" | xargs sed -i "s|$old|$new|"
echo "swap-scenefx-includes: updated:"
echo "$files" | sed 's/^/	/'
