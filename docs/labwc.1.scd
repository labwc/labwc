labwc(1)

# NAME

labwc - A Wayland stacking compositor

# SYNOPSIS

*labwc*  [options...] [command]

# DESCRIPTION

Labwc is a stacking compositor for Wayland. It aims to be light-weight and have
the feel of Openbox albeit with a smaller feature set. Where practicable, it
uses clients to show wall-paper, take screenshots, and so on. It tries to stay
in keeping with wlroots and sway in terms of approach and coding style.

# OPTIONS

*-c* <config>
	Specify a config file

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

# CONFIGURATION AND THEMING

Labwc uses openbox specification for configuration and theming, but does not
support all options. The the following files control the look and behaviour:

- ~/.config/labwc/rc.xml
- ~/.config/labwc/menu.xml
- ~/.config/labwc/autostart
- ~/.config/labwc/environment
- ~/.themes/<name>/openbox-3/themerc

Equivalent XDG Base Directory Specification locations are also honoured.

The configuration file and theme are re-loaded on receiving signal SIGHUP.

The autostart file is executed as a shell script. This is a place for setting a
background image, launching a panel, or similar.

The environment file is parsed as <variable>=<value> and sets environment
variables accordingly. It is recommended to specify keyboard settings here, for
example: `XKB_DEFAULT_LAYOUT=gb`. See xkeyboard-config(7) for details.  Note
that the environment file is treated differently by openbox where it is simply
sourced prior to running openbox.

# SEE ALSO

labwc-config(5), labwc-theme(5), labwc-actions(5)
