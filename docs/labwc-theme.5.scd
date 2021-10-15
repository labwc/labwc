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

*border.width*
	Line width (integer) of border border drawn around window frames.
	Default is 1.

*window.active.border.color*
	Border color of active window

*window.active.title.bg.color*
	Background color for the focussed window's titlebar

*window.inactive.title.bg.color*
	Background color for non-focussed windows' titlebars

*window.active.label.text.color*
	Text color for the focussed window's titlebar

*window.inactive.label.text.color*
	Text color non-focussed windows' titlebars

*window.active.button.unpressed.image.color*
	Color of the images in titlebar buttons in their default, unpressed,
	state. This element is for the focused window.

*window.inactive.button.unpressed.image.color*
	Color of the images in titlebar buttons in their default, unpressed,
	state. This element is for non-focused windows.

Note: The button elements (i.e. window.[in]active.button.\*) support defining
different types of buttons individually by inserting the type ("iconify", "max"
and "close") after the button node. For example:
window.active.button.iconify.unpressed.image.color
This syntax is not not documented on the openbox.org wiki, but is supported by
openbox and is used by many popular themes. For the sake of brevity, these
elements are not listed here, but are supported.

*menu.items.bg.color*
	Background color of inactive menu items

*menu.items.text.color*
	Text color of inactive menu item

*menu.items.active.bg.color*
	Background color of active menu items

*menu.items.active.text.color*
	Text color of active menu item

*osd.bg.color*
	Background color of on-screen-display

*osd.label.text.color*
	Text color of on-screen-display

*border.color*
	Set all border colors. This is obsolete, but supported for backward
	compatibility as some themes still contain it.

# BUTTONS

The images used for the titlebar buttons are 1-bit xbm (X Bitmaps). These are
masks where 0=clear and 1=colored. The xbm image files are placed in the same
directory within your theme as the themerc file. Here are all the possible xbm
files looked for:

- max.xbm
- iconify.xbm
- close.xbm

More will be supported later.

# DEFINITIONS

The handle is the window edge decoration at the bottom of the window.

# DERIVED DIMENSIONS

The window title bar height is equal to the vertical font extents of the title.
Padding will be added to this later.

# SEE ALSO

labwc(1), labwc-config(5), labwc-actions(5)
