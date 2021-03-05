labwc-theme(5)

# NAME

labwc - Theme specification

# THEME

The theme engine aims to be compatible with openbox and themes will be
searched for in the following order:

- ${XDG_DATA_HOME:-$HOME/.local/share}/themes/<theme-name>/openbox-3/
- $HOME/.themes/<theme-name>/openbox-3/
- /usr/share/themes/<theme-name>/openbox-3/
- /usr/local/share/themes/<theme-name>/openbox-3/
- /opt/share/themes/<theme-name>/openbox-3/

Choosing a theme is done by editing the <name> key in the <theme> section of
the rc.xml configuration file (labwc-config(5)).

A theme consists of a themerc file and optionally some xbm icons.

# DATA TYPES

*color RGB values*
	Colors can be specified by hexadecimal RGB values in the format #rrggbb.
	Other formats will be supported later for better openbox theme
	compatibility.

# THEME ELEMENTS

*window.active.title.bg.color*
	Background for the focussed window's titlebar

*window.active.handle.bg.color*
	Background for the focussed window's handle.

*window.inactive.title.bg.color*
	Background for non-focussed windows' titlebars

*window.active.button.unpressed.image.color*
	Color of the images in titlebar buttons in their default, unpressed,
	state. This element is for the focused window.

*window.inactive.button.unpressed.image.color*
	Color of the images in titlebar buttons in their default, unpressed,
	state. This element is for non-focused windows.

*menu.items.bg.color*
	Background color of inactive menu items

*menu.items.text.color*
	Text color of inactive menu item

*menu.items.active.bg.color*
	Background color of active menu items

*menu.items.active.text.color*
	Text color of active menu item

# DEFINITIONS

The handle is the window edge decoration at the bottom of the window.

# DERIVED DIMENSIONS

The window title bar height is equal to the vertical font extents of the title.
Padding will be added to this later.

# SEE ALSO

labwc(1), labwc-config(5), labwc-actions(5)
