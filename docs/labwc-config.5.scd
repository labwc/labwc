labwc-config(5)

# NAME

labwc - Configuration

# CONFIGURATION

The configuration aims to be compatible with openbox specification, but there
are some differences which are pointed out throughout the man pages.

Adhering to XDG Base Directory Specification, configuration files will
be searched for in the following order:

- ${XDG_CONFIG_HOME:-$HOME/.config}/labwc
- ${XDG_CONFIG_DIRS:-/etc/xdg}/labwc

Configuration must be wrapped in a <labwc_config> root-node.

# GENERAL

*<core><decoration>* [server|client]
	Specify server or client side decorations for xdg-shell views. Note
	that it is not always possible to turn off client side decorations.
	Default is server.

*<core><gap>*
	The distance in pixels between views and output edges when using
	movement actions, for example MoveToEdge. Default is 0.

# FOCUS

*<focus><followMouse>* [yes|no]
	Make focus follow mouse, e.g. focus is given to window under mouse
	cursor

*<focus><raiseOnFocus>* [yes|no]
	Also raise window to top when focused. Only applies if followMouse is
	set

# THEME

*<theme><name>*
	The name of the Openbox theme to use. Default is Clearlooks-3.4

*<theme><cornerRadius>*
	The radius of server side decoration top corners. Default is 8.

*<theme><font place="">*
	The font to use for a specific element of a window, menu or OSD.
	Places can be any of:
	- ActiveWindow - titlebar of active window
	- MenuItem - menu item (currently only root menu)
	If no place attribute is provided, the setting will be applied to all
	places.

*<theme><font place=""><name>*
	Describes font name. Default is sans.

*<theme><font place=""><size>*
	Font size in pixels. Default is 10.

# KEYBOARD

*<keyboard><keybind key="">*
	Define a key binding in the format *modifier-key*, where supported
	modifiers include S (shift); C (control); A (alt); W (super). Unlike
	Openbox, multiple space-separated key combinations and key-chains are
	not supported.

*<keyboard><keybind key=""><action name="">*
	Keybind action. See labwc-action(5)

*<keyboard><default />*
	Load the default keybinds listed below. This is an addition to the
	openbox specification and provides a way to keep config files simpler
	whilst allowing your specific keybinds.
	Note that if no rc.xml is found, or if no <keyboard><keybind> entries
	exist, the same default keybinds will be loaded even if the <default />
	element is not provided.

```
  A-Tab - next window
  A-Escape - exit
  W-Return - alacritty
  A-F3 - run bemenu
  A-F4 - close window
  W-a - toggle maximize
  A-<arrow> - move window to edge
  W-<arrow> - resize window to fill half the output
```

	Audio and MonBrightness keys are also bound to amixer and
	brightnessctl respectively

*<keyboard><repeatRate>*
	Sets the rate at which keypresses are repeated per second. The default
	rate is 25/second.

*<keyboard><repeatDelay>*
	Sets the delay before keypresses are repeated in milliseconds.

# MOUSE

*<mouse><doubleClickTime>*
	Set double click time in milliseconds. Default is 500.

*<mouse><context name=""><mousebind button="" action="">*
	Define a mouse binding. Supported context-names include:
	'TitleBar'.

# LIBINPUT

*<libinput><device category="">*
	Define a category of devices to use the configuration values that 
	follow. The category can be set to touch-- devices that define a width 
	and height-- non-touch, default, or the name of a device. You can obtain
	your devices name by running *libinput list-devices* (you may need to 
	be root or a part of the input group to perform this.) Any members of
	this category that aren't set use the default for the device. With the
	exception of tap-to-click, which is enabled by default.

*<libinput><device category=""><naturalscroll>* [yes|no]
	Use natural scrolling for this category if available.

*<libinput><device category=""><lefthanded>* [yes|no]
	Use your devices left-handed mode if available.

*<libinput><device category=""><pointerspeed>* 
	Set the pointer speed for this category. The speed is a number between
	\-1 and 1, with 0 being the default in most cases, and 1 being the
	fastest.

*<libinput><device category=""><accelprofile>* [flat|adaptive]
	Set the pointer's acceleration profile for this category. Flat applies
	no acceleration (the pointers velocity is constant), while adaptive 
	changes the pointers speed based the actual speed of your mouse or 
	finger on your touchpad. 

*<libinput><device category=""><tap>* [yes|no]
	Enable or disable tap-to-click for this category. This is enabled by
	default for all categories.

*<libinput><device category=""><middleemulation>* [yes|no]
	Enable or disable middle button emulation for this category. Middle
	emulation processes a simultaneous left and right click as a press of
	the middle mouse button (scroll wheel).

*<libinput>device category=""><disablewhiletyping>* [yes|no]
	Enable or disable disable while typing for this category. DWT ignores
	any motion events while a keyboard is typing, and for a short while
	after as well. 

# SEE ALSO

labwc(1), labwc-actions(5), labwc-theme(5)
