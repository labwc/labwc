labwc-config(5)

# NAME

labwc - configuration files

# DESCRIPTION

Labwc uses openbox-3.6 specification for configuration and theming, but does not
support all options. The following files form the basis of the labwc
configuration: rc.xml, menu.xml, autostart and environment.

No configuration files are needed to start and run labwc.

In accordance with XDG Base Directory Specification, configuration files are
searched for in the following order:

- ${XDG_CONFIG_HOME:-$HOME/.config}/labwc
- ${XDG_CONFIG_DIRS:-/etc/xdg}/labwc

The configuration directory location can be override with the -C command line
option.

All configuration and theme files except autostart are re-loaded on receiving
signal SIGHUP.

The *autostart* file is executed as a shell script. This is the place for
executing clients for handling background images, panels and similar.

The *environment* file is parsed as *variable=value* and sets environment
variables accordingly. It is recommended to specify keyboard layout settings and
cursor size/theme here; see environment variable section below for details. Note
that the environment file is treated differently by openbox where it is simply
sourced prior to running openbox.

The *menu.xml* file defines the context/root-menus and is described in
labwc-menu(5)

There is a small <theme> section in rc.xml, for example to set rouned corners,
but the remainder of the theme specification and associated files are described
in labwc-theme(5).

*rc.xml* is the main configuration file and all its options are described in
detail below.

# CONFIGURATION

This section describes *rc.xml* configuration options.

## SYNTAX

Configuration must be wrapped in a <labwc_config> root-element like this:

```
<?xml version="1.0"?>
<labwc_config>

<!-- settings -->

</labwc_config>
```

*labwc* parses XML in an element/attribute agnostic way. This is a design
decision to increase config file flexibility and keep code simple. In practical
terms, this means that `<a><b>c</b></a>` is equivalent to `<a b="c" />`.

The following three are therefore treated the same:

```
<action>
  <name>Execute</name>
  <command>foot</command>
</action>
```

```
<action name="Execute">
  <command>foot</command>
</action>
```

```
<action name="Execute" command="foot" />
```

The benefit of the final one is brevity whereas the advantage of the first two
is that you can add ' and " within the `<command>` block, for example:

```
<command>sh -c 'grim -g "`slurp`"'</command>
```

Elements at the same level can have the same name whereas attributes cannot.
Therefore, where multiple objects of the same kind are required (for example
*<action>* and *<keybind>*) the top-node of the object has to be an element.

## CORE

```
<core>
  <decoration>server</decoration>
  <gap>0</gap>
  <adaptiveSync>no</adaptiveSync>
  <reuseOutputMode>no</reuseOutputMode>
</core>
```

*<core><decoration>* [server|client]
	Specify server or client side decorations for xdg-shell views. Note
	that it is not always possible to turn off client side decorations.
	Default is server.

*<core><gap>*
	The distance in pixels between views and output edges when using
	movement actions, for example MoveToEdge. Default is 0.

*<core><adaptiveSync>* [yes|no]
	Enable adaptive sync. Default is no.

*<core><reuseOutputMode>* [yes|no]
	Try to re-use the existing output mode (resolution / refresh rate).
	This may prevent unnecessary screenblank delays when starting labwc
	(also known as flicker free boot). If the existing output mode can not
	be used with labwc the preferred mode of the monitor is used instead.
	Default is no.

## WINDOW SWITCHER

*<windowSwitcher show="" preview="" outlines="">*
	*show* [yes|no] Draw the OnScreenDisplay when switching between
	windows. Default is yes.

	*preview* [yes|no] Preview the contents of the selected window when
	switching between windows. Default is yes.

	*outlines* [yes|no] Draw an outline around the selected window when
	switching between windows. Default is yes.

*<windowSwitcher><fields><field content="" width="%">*
	Define window switcher fields.

	*content* defines what the field shows and can be any of:

		- *type* Show view type ("xdg-shell" or "xwayland")

		- *app_id* Show app_id (class for XWayland)

		- *title* Show window title if different to app_id

	*width* defines the width of the field expressed as a percentage of
	the overall window switcher width. The "%" character is required.

## RESISTANCE

*<resistance><screenEdgeStrength>*
	Screen Edge Strength is how far past the screen's edge your cursor must
	move before the window will move with it. Resistance is counted in
	pixels. Default is 20 pixels.

## FOCUS

*<focus><followMouse>* [yes|no]
	Make focus follow mouse, i.e. focus is given to window under mouse
	cursor. Default is no.

*<focus><followMouseRequiresMovement>* [yes|no]
	Requires cursor movement if followMouse is enabled. It is the same
	as the "underMouse" setting in Openbox. If set to "no", labwc will
	additionally focus the window under the cursor in all situations
	which change the position of a window (e.g. switching workspaces,
	opening/closing windows). Focusing a different window via A-Tab is
	still possible, even with this setting set to "no". Default is yes.

*<focus><raiseOnFocus>* [yes|no]
	Raise window to top when focused. Default is no.

## WINDOW SNAPPING

*<snapping><range>*
	The distance in pixels from the edge of an ouput for window Move
	operations to trigger SnapToEdge. A range of 0 disables window snapping.
	Default is 1.

*<snapping><topMaximize>* [yes|no]
	Maximize window if Move operation ends on the top edge. Default is yes.

## REGIONS

*<regions><region name="snap-1" x="10%" y="10%" width="80%" height="80%">*
	Define snap regions. The regions are calculated based on the usable area
	of each output. Usable area in this context means space not exclusively
	used by layershell clients like panels. The "%" character is required.
	Windows can either be snapped to regions by keeping a keyboard modifier
	pressed while moving a window (Ctrl, Alt, Shift, Logo) or by using the
	SnapToRegion action. By default there are no regions defined.

## WORKSPACES

*<desktops><names><name>*
	Define workspaces. A workspace covers all outputs. The OSD only shows
	windows on the current workspace. Workspaces can be switched to with
	GoToDesktop and windows can be moved with SendToDesktop. See
	labwc-actions(5) for more information about their arguments.

*<desktops><popupTime>*
	Define the timeout after which to hide the workspace OSD.
	A setting of 0 disables the OSD. Default is 1000 ms.

## THEME

*<theme><name>*
	The name of the Openbox theme to use. It is not set by default.

*<theme><cornerRadius>*
	The radius of server side decoration top corners. Default is 8.

*<theme><font place="">*
	The font to use for a specific element of a window, menu or OSD.
	Places can be any of:
	- ActiveWindow - titlebar of active window
	- MenuItem - menu item (currently only root menu)
	- OnScreenDisplay - items in the on screen display
	If no place attribute is provided, the setting will be applied to all
	places.

*<theme><font place=""><name>*
	Describes font name. Default is sans.

*<theme><font place=""><size>*
	Font size in pixels. Default is 10.

*<theme><font place=""><slant>*
	Font slant (normal or italic). Default is normal.

*<theme><font place=""><weight>*
	Font weight (normal or bold). Default is normal.

## KEYBOARD

*<keyboard><keybind key="">*
	Define a key binding in the format *modifier-key*, where supported
	modifiers include S (shift); C (control); A (alt); W (super). Unlike
	Openbox, multiple space-separated key combinations and key-chains are
	not supported. The application "wev" (wayland event viewer) is packaged
	in a lot of distributions and can be used to view all available keynames.

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

## MOUSE

*<mouse><doubleClickTime>*
	Set double click time in milliseconds. Default is 500.

*<mouse><scrollFactor>*
	Set scroll factor. Default is 1.0.

*<mouse><context name=""><mousebind button="" direction=""><action>*
	Multiple *<mousebind>* can exist within one *<context>*; and multiple
	*<action>* can exist within one *<mousebind>*

	Define a mouse binding. Supported context-names include:
	- TitleBar: The decoration on top of the window, where the window
	  buttons and the window title are shown.
	- Title: The area of the titlebar (including blank space) between
	  the window buttons, where the window title is displayed.
	- WindowMenu: The button on the left.
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

	Supported mouse *buttons* are:
	- Left
	- Middle
	- Right

	Supported scroll *directions* are:
	- Up
	- Down
	- Left
	- Right

	Supported mouse actions include:
	- Press: Pressing the specified button down in the context.
	- Release: Releasing the specified button in the context.
	- Click: Pressing and then releasing inside of the the context.
	- DoubleClick: Two presses within the doubleClickTime.
	- Drag: Pressing the button within the context, then moving the cursor.
	- Scroll: Scrolling in specified *direction* in the context.

*<mouse><default />*
	Load default mousebinds. This is an addition to the openbox
	specification and provides a way to keep config files simpler whilst
	allowing user specific binds.  Note that if no rc.xml is found, or if no
	<mouse><mousebind> entries exist, the same default mousebinds will be
	loaded even if the <default /> element is not provided.

## LIBINPUT

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

*<libinput><device category=""><disableWhileTyping>* [yes|no]
	Enable or disable disable while typing for this category. DWT ignores
	any motion events while a keyboard is typing, and for a short while
	after as well.

## WINDOW RULES

Two types of window rules are supported, actions and properties. They are
defined as shown below.

```
<windowRules>

  <!-- Action -->
  <windowRule identifier="" title="">
    <action name=""/>
  </windowRule>

  <!-- Property -->
  <windowRule identifier="" serverDecoration="" />

</windowRules>
```

*Actions*

*<windowRules><windowRule identifier="" title="">*
	Define a window rule for any window which matches the criteria defined
	by the attributes *identifier* or *title*. If both are defined, AND
	logic is used, so both have to match.
	Matching against patterns with '\*' (wildcard) and '?' (joker) is
	supported. Pattern matching is case-insensitive.

	*identifier* relates to app_id for native Wayland windows and WM_CLASS
	for XWayland clients.

	*title* is the title of the window.

*Properties*

Property values can be *yes*, *no* or *default*.

If a window matches criteria for multiple rules which set the same property,
later config entries have higher priority. *default* can be useful in this
situation.

*<windowRules><windowRule serverDecoration="">* [yes|no|default]
	*serverDecoration* over-rules any other setting for server-side window
	decoration on first map.

## ENVIRONMENT VARIABLES

*XCURSOR_THEME* and *XCURSOR_SIZE* are supported to set cursor theme
and size respectively. The default size is 24. System cursor themes can
typically be found with a command such as:

```
find /usr/share/icons/ -type d -name "cursors"
```

The following keyboard-configuration variables are supported:
*XKB_DEFAULT_RULES*, *XKB_DEFAULT_MODEL*, *XKB_DEFAULT_LAYOUT*,
*XKB_DEFAULT_VARIANT* and *XKB_DEFAULT_OPTIONS*.

See xkeyboard-config(7) for details.

# SEE ALSO

labwc(1), labwc-actions(5), labwc-theme(5)
