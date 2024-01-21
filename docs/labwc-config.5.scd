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

When $XDG_CONFIG_HOME is defined, it replaces (rather than augments)
$HOME/.config. The same is the case for $XDG_CONFIG_DIRS and /etc/xdg.

The XDG Base Directory Specification does not specify whether or not programs
should (a) allow the first-identified configuration file to supersede any
others, or (b) define rules for merging the information from more than one file.

By default, labwc uses option (a), reading only the first file identified.  With
the --merge-config option, the search order is reserved, but every configuration
file encountered is processed in turn. Thus, user-specific files will augment
system-wide configurations, with conflicts favoring the user-specific
alternative.

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
Note: Tilde (~) and environment variables in the value are expanded, but
subshell syntax and apostrophes are ignored.

The *menu.xml* file defines the context/root-menus and is described in
labwc-menu(5).

There is a small <theme> section in rc.xml, for example to set rounded corners,
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

## BOOLEANS

Note that in this manual, Boolean values are listed as [yes|no] for simplicity,
but it's also possible to use [true|false] and\/or [on|off];
this is for compatibility with Openbox.

## CORE

```
<core>
  <decoration>server</decoration>
  <gap>0</gap>
  <adaptiveSync>no</adaptiveSync>
  <allowTearing>no</allowTearing>
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

*<core><adaptiveSync>* [yes|no|fullscreen]
	Enable adaptive sync. Default is no.

	*fullscreen* enables adaptive sync whenever a window is in fullscreen
	mode.

*<core><allowTearing>* [yes|no]
	Allow tearing to reduce input lag. Default is no.
	This option requires setting the environment variable
	WLR_DRM_NO_ATOMIC=1.
	*yes* allow tearing if requested by the active window.

*<core><reuseOutputMode>* [yes|no]
	Try to re-use the existing output mode (resolution / refresh rate).
	This may prevent unnecessary screenblank delays when starting labwc
	(also known as flicker free boot). If the existing output mode can not
	be used with labwc the preferred mode of the monitor is used instead.
	Default is no.

## PLACEMENT

*<placement><policy>* [center|automatic|cursor]
	Specify a placement policy for new windows. The "center" policy will
	always place windows at the center of the active output. The "automatic"
	policy will try to place new windows in such a way that they will
	have minimal overlap with existing windows. The "cursor" policy will
	center new windows under the cursor. Default is "center".

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

		- *identifier* Show identifier (app_id for native Wayland
		  windows and WM_CLASS for XWayland clients)

		- *trimmed_identifier* Show trimmed identifier. Trimming removes
		  the first two nodes of 'org.' strings.

		- *title* Show window title if different to app_id

	*width* defines the width of the field expressed as a percentage of
	the overall window switcher width. The "%" character is required.

## RESISTANCE

*<resistance><screenEdgeStrength>*++
*<resistance><windowEdgeStrength>*
	Resist interactive moves and resizes of a window across screen edges or
	the edges of any other window, respectively.

	When an edge strength is positive, it indicates a distance, in pixels,
	that the cursor must move past any relevant encountered edge before an
	interactive move or resize operation will continue across that edge.

	When the strength is negative, any interactive move or resize operation
	that brings the cursor within the absolute value of the specified
	distance, in pixels, from any relevant edge will snap the operation to
	that edge. Thus, as a move or resize approaches an edge, it will
	"attract" the cursor to that edge within the specified distance. As the
	move or resize continues past the edge, it will provide resistance until
	the cursor has moved beyond the distance.

	A strength of zero disables the corresponding resistance effect.

	The default value for both parameters is 20 pixels.

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

The following two options relate to triggering window actions when moving
windows using the mouse.

*<snapping><range>*
	The distance in pixels from the edge of an output for window Move
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

*<desktops number=""><names><name>*
	Define workspaces. A workspace covers all outputs. The OSD only shows
	windows on the current workspace. Workspaces can be switched to with
	GoToDesktop and windows can be moved with SendToDesktop. See
	labwc-actions(5) for more information about their arguments.

	The number attribute defines the minimum number of workspaces. Default
	is 1. The number attribute is optional. If the number attribute is
	specified, names.name is not required.

*<desktops><popupTime>*
	Define the timeout after which to hide the workspace OSD.
	A setting of 0 disables the OSD. Default is 1000 ms.

## THEME

*<theme><name>*
	The name of the Openbox theme to use. It is not set by default.

*<theme><cornerRadius>*
	The radius of server side decoration top corners. Default is 8.

*<theme><keepBorder>* [yes|no]
	Even when disabling server side decorations via ToggleDecorations,
	keep a small border (and resize area) around the window. Default is yes.

*<theme><font place="">*
	The font to use for a specific element of a window, menu or OSD.
	Places can be any of:
	- ActiveWindow - titlebar of active window
	- InactiveWindow - titlebar of all windows that aren't focused by the
	  cursor
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

## MARGIN

*<margin top="" bottom="" left="" right="" output="" />*
	Specify the number of pixels to reserve at the edges of an output
	(typically a dislay/screen/monitor). New, maximized and tiled windows
	will not be placed in these areas. The use-case for *<margin>* is as a
	workaround for clients such as panels that do NOT support the
	wlr-layer-shell protocol.

	*output* is optional; if this attribute is not provided (rather than
	leaving it an empty string) the margin will be applied to all outputs.

## RESIZE

*<resize><popupShow>* [Never|Always|Nonpixel]
	Show a small indicator on top of the window when resizing or moving.
	When the application sets size-hints (usually X11 terminal emulators),
	the indicator will show the dimensions divided by size hints instead.
	In the case of terminal emulators this usually means columns x rows.

	The different values mean:
	- *Never* Do not render the indicator
	- *Always* Render the indicator while moving and resizing windows
	- *Nonpixel* Only render the indicator during resize for windows using
	  size-hints

	Default is Never.

## KEYBOARD

*<keyboard><numlock>* [on|off]
	When recognizing a new keyboard enable or disable Num Lock.
	Default is on.

*<keyboard layoutScope="">* [global|window]
	Stores the keyboard layout either globally or per window and restores
	it when switching back to the window. Default is global.

*<keyboard><keybind key="" layoutDependent="">*
	Define a *key* binding in the format *modifier-key*, where supported
	modifiers are:
	- S (shift)
	- C (control)
	- A or Mod1 (alt)
	- H or Mod3 (hyper)
	- W or Mod4 (super / logo)
	- M or Mod5 (meta)

	Multiple modifiers can be combined like *A-S-f* for Alt-Shift-f.
	The key itself can be any unicode character or a keyname like *Return*.

	Unlike Openbox, multiple space-separated key combinations and key-chains
	are not supported. The application "wev" (wayland event viewer) is
	packaged in a lot of distributions and can be used to view all available
	keynames.

	*layoutDependent* [yes|no]
	Make this specific keybind depend on the currently active keyboard
	layout. If enabled, a keybind using a key which does not exist in
	the currently active layout will not be executed. The physical key
	to trigger a keybind may also change along with the active layout.
	If set to "no" (or is absent) the keybind will be layout agnostic.
	Default is no.

*<keyboard><keybind key=""><action name="">*
	Keybind action. See labwc-action(5).

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
	brightnessctl, respectively.

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

*<mouse><context name=""><mousebind button="" direction="" action=""><action>*
	Multiple *<mousebind>* can exist within one *<context>*; and multiple
	*<action>* can exist within one *<mousebind>*.

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
	- Side
	- Extra
	- Forward
	- Back
	- Task

	Supported scroll *directions* are:
	- Up
	- Down
	- Left
	- Right

	Mouse buttons and directions can be combined with modifier-keys
	(shift (S), super/logo (W), control (C), alt (A), meta (M) and
	hyper (H)), for example:
	<mousebind button="A-Right" action="Press">

	Supported mouse *actions* include:
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

## TOUCH

```
<touch mapToOutput="" />
```

*<touch mapToOutput="" />*
	Direct cursor movement to a specified output. If the compositor is
	running in nested mode, this does not take effect.

## TABLET

```
<tablet mapToOutput="" rotate="0">
  <area top="0.0" left="0.0" width="0.0" height="0.0" />
  <map button="Tip" to="Left" />
  <map button="Stylus" to="Right" />
  <map button="Stylus2" to="Middle" />
</tablet>
```

*<tablet mapToOutput="" />*
	The tablet cursor movement can be restricted to a single output.
	If the output name is left empty or the output does not exists, the
	tablet will span all outputs.

*<tablet rotate="" />* [0|90|180|270]
	The tablet orientation can be changed in 90 degree steps. Default is
	no rotation (0). Rotation will be applied after applying tablet area
	transformation.

*<tablet><area top="mm" left="mm" width="mm" height="mm" />*
	By default the complete tablet area is mapped to the full output.
	The *area* element can be used to truncate the active area of the
	tablet surface. By truncating the active area, it is e.g. possible
	to maintain the same aspect ratio between output and tablet.

	The active tablet area can be specified by setting the *top*/*left*
	coordinate (in mm) and/or *width*/*height* (in mm). If width or
	height are omitted or default (0.0), width/height will be set to
	the remaining width/height seen from top/left.

	Aspect ratio example:
	The dimensions of the tablet are 215mm x 115mm and the output has
	a resolution of 3440x1440. When setting height to "90", because
	215 x 1440 / 3440 = 90, the responsive tablet area height will be
	truncated to match the 21:9 aspect ratio of the output. By
	additionally setting top to "12.5", the active area is centered
	vertically on the tablet surface.

*<tablet><map button="" to="" />*
	Tablet buttons emulate regular mouse buttons. If not specified
	otherwise, the tip (Tip) is mapped to left mouse click, the first pen
	button (Stylus)	is mapped to right mouse button click and the second pen
	button (Stylus2) emulates a middle mouse button click.

	Supported map *buttons* are:
	- Tip
	- Stylus
	- Stylus2
	- Stylus3
	- Pad
	- Pad2..Pad9

	See mouse section above for all supported *to* mouse buttons.

## LIBINPUT

```
<libinput>
  <device category="CATEGORY">
    <naturalScroll></naturalScroll>
    <leftHanded></leftHanded>
    <pointerSpeed></pointerSpeed>
    <accelProfile></accelProfile>
    <tap>yes</tap>
    <tapButtonMap></tapButtonMap>
    <tapAndDrag></tapAndDrag>
    <dragLock></dragLock>
    <middleEmulation></middleEmulation>
    <disableWhileTyping></disableWhileTyping>
  </device>
</libinput>
```

*<libinput><device>*
	Define a new libinput configuration category (profile).

	*CATEGORY* Defines a category of devices (by type or name) to apply the
	settings that follow. The category attribute as optional. If no category
	attribute is provided, a 'default' device profile will created that will
	act as the fallback for all libinput devices. Category can be set to any
	of the following types:
	- *touch* - Devices which have a defined width/height, but do not
	  support multitouch (i.e. they cannot track multiple locations where
	  the screen has been touched). Drawing tablets typically fall into this
	  type.
	- *touchpad* - Same as 'touch' but support multitouch. This typically
	  includes laptop track pads with two-finger scroll and swipe gestures.
	- *non-touch* - Anything not described above, for example traditional
	  mouse pointers.
	- *default* - Defines a device-category applicable to all devices not
	  matched by anything else. This can be useful for a fallback, or if you
	  want the same settings to be applied to all devices.

	If the provided category value is different from all of the above key
	words, it will be used to match the device name directly.

	A list of device names can be obtained by running
	*libinput list-devices* (you may need to be root or a part of the input
	group to perform this).

*<libinput><device><naturalScroll>* [yes|no]
	Use natural scrolling for this category if available.

*<libinput><device><leftHanded>* [yes|no]
	Use your devices left-handed mode if available.

*<libinput><device><pointerSpeed>* [\-1.0 to 1.0]
	Set the pointer speed for this category. The speed is a number between
	\-1.0 and 1.0, with 0.0 being the default in most cases, and 1.0 being
	the fastest.

*<libinput><device><accelProfile>* [flat|adaptive]
	Set the pointer's acceleration profile for this category. Flat applies
	no acceleration (the pointers velocity is constant), while adaptive
	changes the pointers speed based the actual speed of your mouse or
	finger on your touchpad.

*<libinput><device><tap>* [yes|no]
	Enable or disable tap-to-click for this category. This is enabled by
	default for all categories.

*<libinput><device><tapButtonMap>* [lrm|lmr]
	Set the buttons mapped to one-, two-, and three-finger taps to the
	left button, right button, and middle button, respectively (lrm) (the
	default), or to left button, middle button, and right button (lmr).

*<libinput><device><tapAndDrag>* [yes|no]
	Enable or disable tap-and-drag for this category. Tap-and-drag processes
	a tap immediately followed by a finger down as the start of a drag.

*<libinput><device><dragLock>* [yes|no]
	Enable or disable drag lock for this category. Drag lock ignores a
	momentary release of a finger during tap-and-dragging.

*<libinput><device><middleEmulation>* [yes|no]
	Enable or disable middle button emulation for this category. Middle
	emulation processes a simultaneous left and right click as a press of
	the middle mouse button (scroll wheel).

*<libinput><device><disableWhileTyping>* [yes|no]
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

*Criteria*

*<windowRules><windowRule identifier="" title="" matchOnce="">*
	Define a window rule for any window which matches the criteria defined
	by the attributes *identifier* or *title*. If both are defined, AND
	logic is used, so both have to match.
	Matching against patterns with '\*' (wildcard) and '?' (joker) is
	supported. Pattern matching is case-insensitive.

	*identifier* relates to app_id for native Wayland windows and WM_CLASS
	for XWayland clients.

	*title* is the title of the window.

	*matchOnce* can be true|false. If true, the rule will only apply to the
	first instance of the window with the specified identifier or title.

*Properties*

Property values can be *yes*, *no* or *default*.

If a window matches criteria for multiple rules which set the same property,
later config entries have higher priority. *default* can be useful in this
situation.

*<windowRules><windowRule serverDecoration="">* [yes|no|default]
	*serverDecoration* over-rules any other setting for server-side window
	decoration on first map.

*<windowRules><windowRule skipTaskbar="">* [yes|no|default]
	*skipTaskbar* removes window foreign-toplevel protocol handle so that
	it does not appear in clients such as panels and taskbars using that
	protocol.

*<windowRules><windowRule skipWindowSwitcher="">* [yes|no|default]
	*skipWindowSwitcher* removes window from the Window Switcher (alt-tab
	on-screen-display).

*<windowRules><windowRule ignoreFocusRequest="">* [yes|no|default]
	*ignoreFocusRequest* prevent window to activate itself.

*<windowRules><windowRule fixedPosition="">* [yes|no|default]
	*fixedPosition* disallows interactive move/resize and prevents
	re-positioning in response to changes in reserved output space, which
	can be caused by *<margin>* settings or exclusive layer-shell clients
	such as panels.

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
