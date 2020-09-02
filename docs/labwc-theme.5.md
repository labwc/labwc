% labwc-theme(5)
% Johan Malm
% 31 Aug, 2020

# NAME

labwc - Theme specification

# THEME

The theme engine aims to be compatible with openbox and themes will be
searched for in the following order:

- `${XDG_DATA_HOME:-$HOME/.local/share}/themes/<theme-name>/openbox-3/`  
- `$HOME/.themes/<theme-name>/openbox-3/`  
- `/usr/share/themes/<theme-name>/openbox-3/`  
- `/usr/local/share/themes/<theme-name>/openbox-3/`  
- `/opt/share/themes/<theme-name>/openbox-3/`  

Choosing a theme is done by editing the `<name>` key in the `<theme>`
section of your rc.xml (labwc-config(5)).

A theme consists of a themerc file and optionally some xbm icons.

# DATA TYPES

## Color RGB values

Colors can be specified by hexadecimal RGB values in the `#rrggbb`.
Other formats will be supported later for better openbox theme
compatibility.

# THEME ELEMENTS

`window.active.title.bg.color`

:   Background for the focussed window's titlebar

`window.active.handle.bg.color`

:   Background for the focussed window's handle.

`window.inactive.title.bg.color`

:   Background for non-focussed windows' titlebars

`window.active.button.unpressed.image.color`

:   Color of the images in titlebar buttons in their default, unpressed,
    state. This element is for the focused window.

`window.inactive.button.unpressed.image.color`

:   Color of the images in titlebar buttons in their default, unpressed,
    state. This element is for non-focused windows.

# DEFINITIONS

The `handle` is the window decoration placed on the bottom of the window.

# DERIVED DIMENSIONS

The window title bar height is equal to the vertical font extents of the title.
Padding will be added to this later.

# SEE ALSO

labwc(1), labwc-config(5), labwc-actions(5)
