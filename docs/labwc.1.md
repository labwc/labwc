% labwc(1)
% Johan Malm
% 8 Oct, 2020

# NAME

labwc - A Wayland stacking compositor with the look and feel of Openbox

# SYNOPSIS

labwc \[\-c <*config-file*>] \[\-h] \[\-s <*startup-command*>] [-v]  

# DESCRIPTION

Labwc is a [WIP] free, stacking compositor for Wayland. It aims to be light-weight and have the feel of Openbox.

# OPTIONS

`-c <config-file>`

:   Specify path to rc.xml

`-h`

:   Show help message

`-s <startup-command>`

:   Specify startup command

`-v`

:   Increase verbosity. '-v' for info; '-vv' for debug.

# CONFIGURATION AND THEMES

Labwc aims to be compatible with openbox configuration and theming, with the
following files controlling the look and behaviour:

- ~/.config/labwc/rc.xml (see labwc-config(5) for details)  
- ~/.config/labwc/autostart  
- ~/.config/labwc/environment  
- ~/.themes/`<name>`/openbox-3/themerc (see labwc-theme(5) for details)  

Equivalent XDG Base Directory Specification locations are also honoured.

The configuration file and theme are re-loaded on receiving signal SIGHUP.

The autostart file is executed as a shell script. This is a place for setting
a background image, launching a panel, and so on.

The environment file is parsed as `<variable>=<value>` and sets environment
variables accordingly. It is recommended to specify keyboard settings here,
for example: `XKB_DEFAULT_LAYOUT=gb`. See xkeyboard-config(7) for details.

# SEE ALSO

labwc-config(5), labwc-theme(5), labwc-actions(5)
