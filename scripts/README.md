These scripts are intended to be run from the project top-level directory
like this: `scripts/foo.sh`

- `scripts/check`: wrapper to check all files in `src/` and `include/`

- `scripts/checkpatch.pl`: Quick hack on the Linux kernel [checkpatch.pl]
  to lint C files written according to the labwc coding style. Run like
  this: `./checkpatch.pl --no-tree --terse --strict --file <file>`

[checkpatch.pl]: https://raw.githubusercontent.com/torvalds/linux/4ce9f970457899defdf68e26e0502c7245002eb3/scripts/checkpatch.pl
