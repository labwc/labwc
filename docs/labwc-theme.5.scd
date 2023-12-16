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

A theme consists of a themerc file and optionally some titlebar icons (referred
to as buttons).

Theme settings specified in themerc can be overridden by creating a
'themerc-override' file in the configuration directory, which is normally
$HOME/.config/labwc/ but can be a few other locations as described in
labwc-config(5).

# DATA TYPES

*color*
	Colors can be specified by either of the following:
	- #rrggbb (hexadecimal RGB values)
	- #rrggbb aaa (same but with decimal alpha value)

*justification*
	Justification determines the horizontal alignment of text.
	Valid options are Left, Center and Right.

# THEME ELEMENTS

*border.width*
	Line width (integer) of border border drawn around window frames.
	Default is 1.

*padding.height*
	Vertical padding size, used for spacing out elements in the window
	decorations. Default is 3.

*titlebar.height*
	Window title bar height.
	Default equals the vertical font extents of the title plus 2x
	padding.height.

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
	A fixed width can be achieved by setting .min and .max to the same
	value.

*menu.width.max*
	Maximal width for menus. Default is 200.
	A fixed width can be achieved by setting .min and .max to the same
	value.

*window.active.border.color*
	Border color of active window

*window.inactive.border.color*
	Border color of inactive window

*window.active.indicator.toggled-keybind.color*
	Status indicator for the ToggleKeybinds action. Can be set to the same
	value as set for window.active.border.color to disable the status
	indicator.

*window.active.title.bg.color*
	Background color for the focused window's titlebar

*window.inactive.title.bg.color*
	Background color for non-focused windows' titlebars

*window.active.label.text.color*
	Text color for the focused window's titlebar

*window.inactive.label.text.color*
	Text color non-focused windows' titlebars

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
different types of buttons individually by inserting the type ("menu",
"iconify", "max" and "close") after the button node. For example:
window.active.button.iconify.unpressed.image.color
This syntax is not documented on the openbox.org wiki, but is supported by
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
	Border width of on-screen-display. Inherits *border.width* if not set.

*osd.label.text.color*
	Text color of on-screen-display

*osd.window-switcher.width*
	Width of window switcher in pixels. Default is 600.

*osd.window-switcher.padding*
	Padding of window switcher in pixels. This is the space between the
	window-switcher border and its items. Default is 4.

*osd.window-switcher.item.padding.x*
	Horizontal padding of window switcher entries in pixels.
	Default is 10.

*osd.window-switcher.item.padding.y*
	Vertical padding of window switcher entries in pixels.
	Default is 1.

*osd.window-switcher.item.active.border.width*
	Border width of the selection box in the window switcher in pixels.
	Default is 2.

*osd.workspace-switcher.boxes.width*
	Width of boxes in workspace switcher in pixels. Setting to 0 disables
	boxes. Default is 20.

*osd.workspace-switcher.boxes.height*
	Height of boxes in workspace switcher in pixels. Setting to 0 disables
	boxes. Default is 20.

*border.color*
	Set all border colors. This is obsolete, but supported for backward
	compatibility as some themes still contain it.

# BUTTONS

The images used for the titlebar icons are referred to as buttons.

The image formats listed below are supported. They are listed in order of
precedence, where the first format in the list is searched for first.

- png
- svg
- xbm

By default, buttons are 1-bit xbm (X Bitmaps). These are masks where 0=clear and
1=colored. The xbm image files are placed in the same directory as the themerc
file within a particular theme. The following xbm buttons are supported:

- max.xbm
- iconify.xbm
- close.xbm
- menu.xbm
- max_toggled.xbm

Additional icons can be defined to be shown when the mouse pointer is hovering
over the button in question:

- max_hover.xbm
- iconify_hover.xbm
- close_hover.xbm
- menu_hover.xbm
- max_toggled_hover.xbm

One advantage of xbm buttons over other formats is that they change color based
on the theme. Other formats use the suffices "-active" and "-inactive" to align
with the respective titlebar colors. For example: "close-active.png"

For compatibility reasons, the following alternative names are supported
for xbm files:

- max_hover_toggled.xbm for max_toggled_hover.xbm

When using png or svg icons, for a full theme experience all of the
following icons should be added:

- close-active.[png|svg]
- close_hover-active.[png|svg]
- close_hover-inactive.[png|svg]
- close-inactive.[png|svg]
- iconify-active.[png|svg]
- iconify_hover-active.[png|svg]
- iconify_hover-inactive.[png|svg]
- iconify-inactive.[png|svg]
- max-active.[png|svg]
- max_hover-active.[png|svg]
- max_hover-inactive.[png|svg]
- max-inactive.[png|svg]
- max_toggled-active.[png|svg]
- max_toggled_hover-active.[png|svg]
- max_toggled_hover-inactive.[png|svg]
- max_toggled-inactive.[png|svg]
- menu-active.[png|svg]
- menu_hover-active.[png|svg]
- menu_hover-inactive.[png|svg]
- menu-inactive.[png|svg]

# DEFINITIONS

The handle is the window edge decoration at the bottom of the window.

# SEE ALSO

labwc(1), labwc-config(5), labwc-actions(5)
