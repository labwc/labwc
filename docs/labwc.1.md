% labwc(1)
% Johan Malm
% 18 July, 2020

# NAME

labwc - A Wayland stacking compositor with the look and feel of Openbox

# SYNOPSIS

labwc \[\-s <*startup-command*>] \[\-c <*config-file*>]  

# DESCRIPTION

Labwc is a [WIP] free, stacking compositor for Wayland. It aims to be light-weight and have the feel of Openbox.

# OPTIONS

`-s <startup-command>`

:   Specify startup command

`-c <config-file>`

:   Specify path to rc.xml

# CONFIGURATION FILES

If no file is specified using the -c option, the XDG Base Directory
Specification is adhered to using `labwc` in preference to `openbox`.

Configuration files will be searched for in the following order:

- `${XDG_CONFIG_HOME:-$HOME/.config}/labwc`  
- `${XDG_CONFIG_DIRS:-/etc/xdg}/labwc`  
- `${XDG_CONFIG_HOME:-$HOME/.config}/openbox`  
- `${XDG_CONFIG_DIRS:-/etc/xdg}/openbox`  

See labwc(5) for rc.xml options.

# THEME

The theme engine aims to be compatible with openbox themes.

Themes will be searched for in the following order:

- `${XDG_DATA_HOME:-$HOME/.local/share}/themes`  
- `$HOME/.themes`  
- `/usr/share/themes`  
- `/usr/local/share/themes`  
- `/opt/share/themes`  

See labwc(5) for theme options.

# SEE ALSO

- `labwc(5)`

