labwc-theme(5)

# NAME

labwc - theme files

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

*justification*
	Justification determines the horizontal alignment of text. Valid options
	are Left, Center and Right.

# THEME ELEMENTS

*border.width*
	Line width (integer) of border border drawn around window frames.
	Default is 1.

*padding.height*
	Vertical padding size, used for spacing out elements in the window decorations.
	Default is 3.

*menu.items.padding.x*
	Horizontal padding of menu text entries in pixels.
	Default is 7.

*menu.items.padding.y*
	Vertical padding of menu text entries in pixels.
	Default is 4.

*menu.overlap.x*
	Horizontal overlap in pixels between submenus and their parents. A
	positive value move submenus over the top of their parents, whereas a
	negative value creates a gap between submenus and their parents.
	Default is 0.

*menu.overlap.y*
	Vertical offset in pixels between submenus and their parents. Positive
	values for downwards and negative for upwards. Default is 0.

*menu.width.min*
	Minimal width for menus. Default is 20.
	A fixed width can be achieved by setting .min and .max to the same value.

*menu.width.max*
	Maximal width for menus. Default is 200.
	A fixed width can be achieved by setting .min and .max to the same value.

*window.active.border.color*
	Border color of active window

*window.inactive.border.color*
	Border color of inactive window

*window.active.title.bg.color*
	Background color for the focussed window's titlebar

*window.inactive.title.bg.color*
	Background color for non-focussed windows' titlebars

*window.active.label.text.color*
	Text color for the focussed window's titlebar

*window.inactive.label.text.color*
	Text color non-focussed windows' titlebars

*window.label.text.justify*
	Specifies how window titles are aligned in the titlebar for both
	focused and unfocused windows. Type justification. Default Left.

*window.active.button.unpressed.image.color*
	Color of the images in titlebar buttons in their default, unpressed,
	state. This element is for the focused window.

*window.inactive.button.unpressed.image.color*
	Color of the images in titlebar buttons in their default, unpressed,
	state. This element is for non-focused windows.

Note: The button elements (i.e. window.[in]active.button.\*) support defining
different types of buttons individually by inserting the type ("menu", "iconify", "max"
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

*menu.separator.width*
	Line thickness of menu separators. Default 1.

*menu.separator.padding.width*
	Space on the left and right side of each separator line. Default 6.

*menu.separator.padding.height*
	Space above and below each separator line. Default 3.

*menu.separator.color*
	Menu separator color. Default #888888.

*osd.bg.color*
	Background color of on-screen-display

*osd.border.color*
	Border color of on-screen-display

*osd.border.width*
	Border width of on-screen-display

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
- menu.xbm

More will be supported later.

Note: menu.xbm is not part of openbox-3.6 spec

# DEFINITIONS

The handle is the window edge decoration at the bottom of the window.

# DERIVED DIMENSIONS

The window title bar height is equal to the vertical font extents of the title.
Padding will be added to this later.

# SEE ALSO

labwc(1), labwc-config(5), labwc-actions(5)
