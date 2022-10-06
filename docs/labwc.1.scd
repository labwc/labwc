labwc(1)

# NAME

labwc - a wayland stacking compositor

# SYNOPSIS

*labwc*  [options...]

# DESCRIPTION

Labwc is a wlroots-based stacking compositor for wayland.

It is light-weight and independent with a focus on simply stacking windows
well and rendering some window decorations. Where practicable it uses clients
for wall-paper, panels, screenshots and so on.

The compositor will exit or reload its configuration upon receiving SIGTERM
and SIGHUP respectively.  For example:

```
kill -s <signal> $LABWC_PID
killall -s <signal> labwc
```

# OPTIONS

*-c, --config* <config-file>
	Specify a config file with path

*-C, --config-dir* <config-directory>
	Specify a config directory

*-d, --debug*
	Enable full logging, including debug information

*-h, --help*
	Show help message and quit

*-s, --startup* <command>
	Run command on startup

*-v, --version*
	Show the version number and quit

*-V, --verbose*
	Enable more verbose logging

# SEE ALSO

labwc-config(5), labwc-theme(5), labwc-actions(5)
