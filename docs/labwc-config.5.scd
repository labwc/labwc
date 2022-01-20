labwc-config(5)

# NAME

labwc - Configuration

# CONFIGURATION

The configuration aims to be compatible with openbox specification, but there
are some differences which are pointed out throughout the man pages.

In accordance with XDG Base Directory Specification, configuration files will
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

*<core><adaptiveSync>* [yes|no]
	Enable adaptive sync. Default is no.

*<core><cycleViewPreview>* [yes|no]
	Preview the contents of the selected window when cycling between windows.
	Default is no.

# FOCUS

*<focus><followMouse>* [yes|no]
	Make focus follow mouse, i.e. focus is given to window under mouse
	cursor. Default is no.

*<focus><raiseOnFocus>* [yes|no]
	Raise window to top when focused. Default is no.

# THEME

*<theme><name>*
	The name of the Openbox theme to use. It is not set by default.

*<theme><cornerRadius>*
	The radius of server side decoration top corners. Default is 8.

*<theme><font place="">*
	The font to use for a specific element of a window, menu or OSD.
	Places can be any of:
	- ActiveWindow - titlebar of active window
	- MenuItem - menu item (currently only root menu)
	- OSD - items in the on screen display
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
	Set the rate at which keypresses are repeated per second.
	Default is 25.

*<keyboard><repeatDelay>*
	Set the delay before keypresses are repeated in milliseconds.
	Default is 600.

# MOUSE

*<mouse><doubleClickTime>*
	Set double click time in milliseconds. Default is 500.

*<mouse><context name=""><mousebind button=""><action>*
	Multiple *<mousebind>* can exist within one *<context>*; and multiple
	*<action>* can exist within one *<mousebind>*

	Define a mouse binding. Supported context-names include:
	- TitleBar: The area where the title of the window is shown.
	- Title: The title of the window itself.
	- Iconify: The button that looks like an underline.
	- Maximize: The button that looks like a box.
	- Close: The button that looks like an X.
	- Top: The top edge of the window's border.
	- Bottom: The bottom edge of the window's border.
	- Left: The left edge of the window's border.
	- Right: The right edge of the window's border.
	- TRCorner: The top-right corner of the window's border.
	- TLCorner: The top-left corner of the window's border.
	- BLCorner: The bottom-left corner of the window's border.
	- BRCorner: The bottom-right edge of the window's border.
	- Client: The client area of a window, inside its decorations.
	  Events bound to Client are also passed to applications.
	- Frame: Any part of a window, but events bound to Frame are not passed
	  through to the application.
	- Desktop: The desktop background, where no windows are present.
	- Root: A synonym for Desktop (for compatibility).

	Supported mouse buttons are:
	- Left
	- Middle
	- Right

	Supported mouse actions include:
	- Press: Pressing the specified button down in the context.
	- Release: Releasing the specified button in the context.
	- Click: Pressing and then releasing inside of the the context.
	- DoubleClick: Two presses within the doubleClickTime.
	- Drag: Pressing the button within the context, then moving the cursor

# LIBINPUT

*<libinput><device category="">*
	Define a category of devices to use the configuration values that 
	follow. The category can be set to touch (devices that define a width
	and height), non-touch, default, or the name of a device. You can obtain
	your devices name by running *libinput list-devices* (you may need to 
	be root or a part of the input group to perform this.) Any members of
	this category that are not set use the default for the device. With the
	exception of tap-to-click, which is enabled by default.

*<libinput><device category=""><naturalScroll>* [yes|no]
	Use natural scrolling for this category if available.

*<libinput><device category=""><leftHanded>* [yes|no]
	Use your devices left-handed mode if available.

*<libinput><device category=""><pointerSpeed>* [\-1.0 to 1.0]
	Set the pointer speed for this category. The speed is a number between
	\-1.0 and 1.0, with 0.0 being the default in most cases, and 1.0 being
	the fastest.

*<libinput><device category=""><accelProfile>* [flat|adaptive]
	Set the pointer's acceleration profile for this category. Flat applies
	no acceleration (the pointers velocity is constant), while adaptive 
	changes the pointers speed based the actual speed of your mouse or 
	finger on your touchpad. 

*<libinput><device category=""><tap>* [yes|no]
	Enable or disable tap-to-click for this category. This is enabled by
	default for all categories.

*<libinput><device category=""><tapButtonMap>* [lrm|lmr]
	Set the buttons mapped to one-, two-, and three-finger taps to the
	left button, right button, and middle button, respectively (lrm) (the
	default), or to left button, middle button, and right button (lmr).

*<libinput><device category=""><middleEmulation>* [yes|no]
	Enable or disable middle button emulation for this category. Middle
	emulation processes a simultaneous left and right click as a press of
	the middle mouse button (scroll wheel).

*<libinput>device category=""><disableWhileTyping>* [yes|no]
	Enable or disable disable while typing for this category. DWT ignores
	any motion events while a keyboard is typing, and for a short while
	after as well. 


# RESISTANCE

*<resistance><screenEdgeStrength>*
	Screen Edge Strength is how far past the screen's edge your cursor must
	move before the window will move with it. Resistance is counted in
	pixels. Default is 20 pixels.

# SEE ALSO

labwc(1), labwc-actions(5), labwc-theme(5)
