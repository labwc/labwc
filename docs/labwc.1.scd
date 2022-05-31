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

# OPTIONS

*-c* <config-file>
	Specify a config file with path

*-C* <config-directory>
	Specify a config directory

*-d*
	Enable full logging, including debug information

*-h*
	Show help message and quit

*-s* <command>
	Run command on startup

*-v*
	Show the version number and quit

*-V*
	Enable more verbose logging

# SEE ALSO

labwc-config(5), labwc-theme(5), labwc-actions(5)
