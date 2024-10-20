labwc-config(5)

# NAME

labwc - configuration files

# DESCRIPTION

Labwc uses openbox-3.6 specification for configuration and theming, but does not
support all options. The following files form the basis of the labwc
configuration: rc.xml, menu.xml, autostart, shutdown, environment and xinitrc.

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

The configuration directory location can be overridden with the -C command line
option.

All configuration and theme files except autostart and shutdown are re-loaded on
receiving signal SIGHUP.

Environment variables may be set within *environment* files, wherein each line
defines shell variables in the format *variable=value*. It is recommended to
specify keyboard layout settings and cursor size/theme here; see environment
variable section below for details. Within an XDG Base Directory, a file named
"environment" will be parsed first, followed by any file matching the glob
"environment.d/\*.env". Files within the environment.d directory are parsed in
alphabetical order. Unless the --merge-config option is specified, labwc will
consider a particular XDG Base Directory to have provided an environment file if
that directory contains either the "environment" file or at least one
"environment.d/\*.env" file.

Note: environment files are treated differently by Openbox, which will simply
source the file as a valid shell script before running the window manager. Files
are instead parsed directly by labwc, although any environment variables
referenced as $VARIABLE or ${VARIABLE} will be substituted and the tilde (~)
will be expanded as the user's home directory.

The *autostart* file is executed as a shell script after labwc has read its
configuration and set variables defined in the environment file. Additionally,
the environment variables WAYLAND_DISPLAY and (when labwc is built with Xwayland
support) DISPLAY will be defined. This is the place for executing clients for
handling background images, panels and other tasks that should run automatically
when labwc launches.

The *shutdown* file is executed as a shell script when labwc is preparing to
terminate itself. All environment variables, including WAYLAND_DISPLAY and
DISPLAY, will be available to the script. However, because the script runs
asynchronously with other termination tasks, the shutdown file should not assume
that the display will be usable. This file is useful to perform any custom
operations necessary to finalize a labwc session.

The *menu.xml* file defines the context/root-menus and is described in
labwc-menu(5).

There is a small <theme> section in rc.xml, for example to set rounded corners,
but the remainder of the theme specification and associated files are described
in labwc-theme(5).

*rc.xml* is the main configuration file and all its options are described in
detail below.

The *xinitrc* file is executed as a shell script whenever labwc launches the
Xwayland X11 server. This may happen multiple times throughout the session if
Xwayland is not configured to persist when no X11 clients are connected.

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
terms, this means that `<a><b>c</b></a>` is usually equivalent to `<a b="c" />`.
However, there are some caveats:

	1. In menu.xml, the attributes *id*, *label* and *execute* must be
	expressed as attributes rather than as separate child elements.

	2. Ordering of attributes can sometimes be significant. For example,
	when setting fonts via attributes rather than children,

		*<font place="ActiveWindow" name="..." size="..." />*

	will set a font for active window titles, while

		*<font name="..." size="..." place="ActiveWindow" />*

	will set a font as if *place* were unspecified, because *name* and
	*size* will be processed before *place*.

In general, it is recommended that attributes not be used to collapse more than
one level of the configuration hierarchy.

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

*<core><allowTearing>* [yes|no|fullscreen|fullscreenForced]
	Allow tearing to reduce input lag. Default is no.

	*yes* allows tearing if requested by the active window.

	*fullscreen* allows tearing if requested by the active window, but
	only when the window is in fullscreen mode.

	*fullscreenForced* enables tearing whenever the active window is in
	fullscreen mode, whether or not the application has requested tearing.

	Use the *ToggleTearing* action for forcefully enable tearing.

	Note: Enabling this option with atomic mode setting is experimental. If
	you experience undesirable side effects when tearing is allowed,
	consider setting the environment variable WLR_DRM_NO_ATOMIC=1 when
	launching labwc.

*<core><reuseOutputMode>* [yes|no]
	Try to re-use the existing output mode (resolution / refresh rate).
	This may prevent unnecessary screenblank delays when starting labwc
	(also known as flicker free boot). If the existing output mode can not
	be used with labwc the preferred mode of the monitor is used instead.
	Default is no.

*<core><xwaylandPersistence>* [yes|no]
	Keep XWayland alive even when no clients are connected, rather than
	using a "lazy" policy that allows the server to launch on demand and die
	when it is no longer needed. Default is no.

	Note: changing this setting requires a restart of labwc.

## PLACEMENT

*<placement><policy>* [center|automatic|cursor|cascade]
	Specify a placement policy for new windows. The "center" policy will
	always place windows at the center of the active output. The "automatic"
	policy will try to place new windows in such a way that they will
	have minimal overlap with existing windows. The "cursor" policy will
	center new windows under the cursor. The "cascade" policy will try to
	place new windows at the center of the active output, but possibly
	shifts its position to bottom-right not to cover existing windows.
	Default is "center".

*<placement><cascadeOffset><x>*++
*<placement><cascadeOffset><y>*
	Specify the offset by which a new window can be shifted from an existing
	window when <placement><policy> is "cascade". These values must be positive.
	Default is the height of titlebar (the sum of *titlebar.height* and
	*border.width* from theme) plus 5 for both *x* and *y*.

## WINDOW SWITCHER

*<windowSwitcher show="" preview="" outlines="" allWorkspaces="">*
	*show* [yes|no] Draw the OnScreenDisplay when switching between
	windows. Default is yes.

	*preview* [yes|no] Preview the contents of the selected window when
	switching between windows. Default is yes.

	*outlines* [yes|no] Draw an outline around the selected window when
	switching between windows. Default is yes.

	*allWorkspaces* [yes|no] Show windows regardless of what workspace
	they are on. Default no (that is only windows on the current workspace
	are shown).

*<windowSwitcher><fields><field content="" width="%">*
	Define window switcher fields.

	*content* defines what the field shows and can be any of:

		- *type* Show view type ("xdg-shell" or "xwayland")

		- *identifier* Show identifier (app_id for native Wayland
		  windows and WM_CLASS for XWayland clients)

		- *trimmed_identifier* Show trimmed identifier. Trimming removes
		  the first two nodes of 'org.' strings.

		- *title* Show window title if different to app_id

		- *workspace* Show workspace name

		- *state* Show window state, M/m/F (max/min/full)

		- *type_short* Show view type ("W" or "X")

		- *output* Show output id, if more than one output detected

		- *custom* A printf style config that can replace all the above
		  fields are:
			- 'B' - shell type, values [xwayland|xdg-shell]
			- 'b' - shell type (short form), values [X|W]
			- 'S' - state of window, values [M|m|F] (3 spaces allocated)
			        (maximized, minimized, fullscreen)
			- 's' - state of window (short form), values [M|m|F] (1 space)
			- 'I' - wm-class/app-id
			- 'i' - wm-class/app-id trimmed, remove "org." if available
			- 'W' - workspace name
			- 'w' - workspace name (if more than 1 ws configured)
			- 'O' - output name
			- 'o' - output name (show if more than 1 monitor active)
			- 'T' - title of window
			- 't' - title of window (if different than wm-class/app-id)
		  Recommend using with a monospace font, to keep alignment.
		- *custom - subset of printf options allowed -- man 3 printf*
			- random text may be inserted
			- field length, example "%10" use 10 spaces, even if text uses less
			- left justify text, example "%-"
			- right justify text, example "%" instead of "%-"
			- example, %-10 would left justify and make room for 10 characters
		- Only one custom format allowed now. Future enhancements may
		  allow more than one.

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

*<resistance><unSnapThreshold>*
	Sets the movement of cursor in pixel required for a tiled or maximized
	window to be moved with an interactive move. Default is 20.

*<resistance><unMaximizeThreshold>*
	Sets the one-dimentional movement of cursor in pixel required for a
	*vertically or horizontally* maximized window to be moved with an
	interactive move. Default is 150.

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

Windows may be "snapped" to an edge or user-defined region of an output when
activated with SnapToEdge actions or, optionally, by dragging windows to the
edges of an output. Edge snapping causes a window to occupy half of its output,
extending outward from the snapped edge.

*<snapping><range>*
	If an interactive move ends with the cursor a maximum distance *range*,
	(in pixels) from the edge of an output, the move will trigger a
	SnapToEdge action for that edge. A *range* of 0 disables snapping via
	interactive moves. Default is 1.

*<snapping><overlay><enabled>* [yes|no]
	Show an overlay when snapping to a window to an edge. Default is yes.

*<snapping><overlay><delay><inner>*++
*<snapping><overlay><delay><outer>*
	Sets the delay to show an overlay when snapping a window to each type of
	edge. Defaults are 500 ms.
	*inner* edges are edges with an adjacent output and *outer* edges are
	edges without an adjacent output.

*<snapping><topMaximize>* [yes|no]
	If *yes*, an interactive move that snaps a window to the top edge will
	maximize the window. If *no*, snapping will behave as it does with other
	edges, causing the window to occupy the top half of an output. Default
	is yes.

*<snapping><notifyClient>* [always|region|edge|never]
	Snapping windows can trigger corresponding tiling events for native
	Wayland clients. Clients may use these events to alter their rendering
	based on knowledge that some edges of the view are confined to edges of
	a snapping region or output. For example, rounded corners may become
	square when tiled, or media players may letter-box or pillar-box video
	rather than imposing rigid aspect ratios on windows that will violate
	the constraints of window snapping.

	- When *always* is specified, any window that is snapped to either an
	  output edge or a user-defined region will receive a tiling event.

	- When *region* is specified, only windows snapped to a user-defined
	  region will receive an event.

	- When *edge* is specified, only windows snapped to an output edge will
	  receive an event.

	- When *never* is specified, tiling events will never be triggered.

	The default is "always".

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
	Define workspaces. A workspace covers all outputs. Workspaces can be
	switched to with GoToDesktop and windows can be moved with
	SendToDesktop.
	See labwc-actions(5) for more information about their arguments.

	The number attribute defines the minimum number of workspaces. Default
	is 1. The number attribute is optional. If the number attribute is
	specified, names.name is not required.

*<desktops><popupTime>*
	Define the timeout after which to hide the workspace OSD.
	A setting of 0 disables the OSD. Default is 1000 ms.

*<desktops><prefix>*
	Set the prefix to use when using "number" above. Default is "Workspace"

## THEME

*<theme><name>*
	The name of the Openbox theme to use. It is not set by default.

*<theme><icon>*
	The name of the icon theme to use. It is not set by default.

*<theme><titlebar><layout>*
	Selection and order of buttons in a window's titlebar.
	The following identifiers can be used, each only once:
	- 'icon': window icon
	- 'menu': window menu
	- 'iconify': iconify
	- 'max': maximize toggle
	- 'close': close
	- 'shade': shade toggle
	- 'desk': all-desktops toggle

	A colon deliminator is used to separate buttons on the left and right,
	whereas commas are used to separate items within a section. It is
	mandatory to use one colon.

	Default: icon:iconify,max,close

*<theme><titlebar><showTitle>* [yes|no]
	Show the window title in the titlebar. Default is yes.

*<theme><cornerRadius>*
	The radius of server side decoration top corners. Default is 8.

*<theme><keepBorder>* [yes|no]
	Even when disabling server side decorations via ToggleDecorations,
	keep a small border (and resize area) around the window. Default is yes.

*<theme><dropShadows>* [yes|no]
	Should drop-shadows be rendered behind windows. Default is no.

*<theme><font place="">*
	The font to use for a specific element of a window, menu or OSD.
	Places can be any of:
	- ActiveWindow - titlebar of active window
	- InactiveWindow - titlebar of all windows that aren't focused by the
	  cursor
	- MenuHeader - menu title
	- MenuItem - menu item (currently only root menu)
	- OnScreenDisplay - items in the on screen display
	If no place attribute is provided, the setting will be applied to all
	places.

*<theme><font place=""><name>*
	Describes font name. Default is sans.

*<theme><font place=""><size>*
	Font size in pixels. Default is 10.

*<theme><font place=""><slant>*
	Font slant (normal, oblique or italic). Default is normal.

*<theme><font place=""><weight>*
	Font weight (normal or bold). Default is normal.

## MARGIN

*<margin top="" bottom="" left="" right="" output="" />*
	Specify the number of pixels to reserve at the edges of an output
	(typically a display/screen/monitor). New, maximized and tiled windows
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

*<resize><drawContents>* [yes|no]
	Let the application redraw its contents while resizing. If disabled, an
	outlined rectangle is shown to indicate the geometry of resized window.
	Default is yes.

## KEYBOARD

*<keyboard><numlock>* [on|off]
	When recognizing a new keyboard enable or disable Num Lock.
	Default is on.

*<keyboard layoutScope="">* [global|window]
	Stores the keyboard layout either globally or per window and restores
	it when switching back to the window. Default is global.

*<keyboard><keybind key="" layoutDependent="" onRelease="" allowWhenLocked="">*
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

	*allowWhenLocked* [yes|no]
	Make this keybind work even if the screen is locked. Default is no.

	*onRelease* [yes|no]
	When yes, fires the keybind action when the key or key
	combination is released, rather than first pressed. This is useful to
	bind actions to only modifier keys, where the action should fire when
	the modifier is used without another key. Default is no.

	The example below will trigger the launch of rofi when the super key is
	pressed & released, without interference from other multi-key
	combinations that include the super key:

	```
	<keybind key="Super_L" onRelease="yes">
	  <action name="Execute" command="rofi -show drun"/>
	</keybind>
	```

*<keyboard><keybind key=""><action name="">*
	Keybind action. See labwc-actions(5).

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

*<mouse><context name=""><mousebind button="" direction="" action=""><action>*
	Multiple *<mousebind>* can exist within one *<context>*; and multiple
	*<action>* can exist within one *<mousebind>*.

	Define a mouse binding. Supported context-names include:
	- TitleBar: The decoration on top of the window, where the window
	  buttons and the window title are shown.
	- Title: The area of the titlebar (including blank space) between
	  the window buttons, where the window title is displayed.
	- Icon: A window icon that, by default, displays a window menu.
	- WindowMenu: A button that, by default, displays a window menu.
	- Iconify: A button that, by default, iconifies a window.
	- Maximize: A button that, by default, toggles maximization of a window.
	- Shade: A button that, by default, toggles window shading.
	- AllDesktops: A button that, by default, toggles omnipresence of a window.
	- Close: A button that, by default, closses a window.
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
	- All: Anywhere on the screen.

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
<touch deviceName="" mapToOutput="" mouseEmulation="no"/>
```

*<touch deviceName="" />*
	A touch configuration can be bound to a specific device. If device
	name is left empty, the touch configuration applies to all touch
	devices or functions as a fallback. Multiple touch configurations
	can exist.
	See the libinput device section for obtaining the device names.

*<touch mapToOutput="" />*
	Direct cursor movement to a specified output. If the compositor is
	running in nested mode, this does not take effect.

*<touch mouseEmulation="" />*
	If mouseEmulation is enabled, all touch up/down/motion events are
	translated to mouse button and motion events.

## TABLET

```
<tablet mapToOutput="" rotate="0" mouseEmulation="no">
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

	The tablet cursor automatically applies the orientation/rotation of
	a mapped output for absolute motion.

*<tablet rotate="" />* [0|90|180|270]
	The tablet orientation can be changed in 90 degree steps. Default is
	no rotation (0). Rotation will be applied before applying tablet area
	transformation.

	See also *calibrationMatrix* in libinput section below for advanced
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

*<tablet mouseEmulation="" />* [yes|no]
	The tablet can be forced to always use mouse emulation. This prevents
	tablet specific restrictions, e.g. no support for drag-and-drop, but
	also omits tablet specific features like reporting pen pressure.

	Use the *ToggleTabletMouseEmulation* action for toggling between
	mouse emulation on and off.

*<tablet><map button="" to="" />*
	Pen and pad buttons behave like regular mouse buttons.With mouse
	emulation set to "no", which is the default, and if not	specified
	otherwise, the first pen button is mapped to the right mouse button,
	the second pen button to the middle mouse button and a third pen
	button is mapped to the side mouse button.

	Supported map *buttons* are:
	- Stylus
	- Stylus2
	- Stylus3

	The stylus buttons can be mapped *to*:
	- Right
	- Middle
	- Side

	The tip cannot be remapped.

	When using mouse emulation, all pen buttons emulate regular mouse
	buttons. The tip, stylus and pad buttons can be mapped to all
	available mouse	buttons. If not specified otherwise, the tip is
	mapped to left mouse click, the first pen button (Stylus) is mapped
	to right mouse button click and the second pen button (Stylus2)
	emulates a middle mouse	button click.

	Supported map *buttons* for mouse emulation are:
	- Tip
	- Stylus
	- Stylus2
	- Stylus3
	- Pad
	- Pad2..Pad9

	See mouse section above for all supported mouse buttons.

## TABLET TOOL

```
<tabletTool motion="absolute" relativeMotionSensitivity="1" />
```

*<tabletTool motion="">* [absolute|relative]
	All tablet tools, except of type mouse and lens, use "absolute"
	positioning by default. The *motion* attribute allows to set tools
	to relative motion instead. Positioning for a tablet mouse or
	tablet lens cannot be changed, those tools always use relative mode.

*<tabletTool relativeMotionSensitivity="">*
	When using relative motion, *relativeMotionSensitivity* controls
	the speed of the cursor. Using a value lower than 1.0 decreases the
	speed, using a value greater than 1.0 increases the speed of the
	cursor. The default is "1.0".

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
    <clickMethod></clickMethod>
    <sendEventsMode></sendEventsMode>
    <calibrationMatrix></calibrationMatrix>
    <scrollFactor>1.0</scrollFactor>
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

*<libinput><device><clickMethod>* [none|buttonAreas|clickfinger]
	Configure the method by which physical clicks on a touchpad are mapped
	to mouse-button events.

	The click methods available are:
	- *buttonAreas* - The bottom of the touchpad is divided into distinct
	  regions corresponding to left, middle and right buttons; clicking
	  within the region will trigger the corresponding event. Clicking the
	  main area further up produces a left button event.
	- *clickfinger* - Clicking with one, two or three finger(s) will produce
	  left, right or middle button event without regard to the location of a
	  click.
	- *none* - Physical clicks will not produce button events.

	The default method depends on the touchpad hardware.

*<libinput><device><sendEventsMode>* [yes|no|disabledOnExternalMouse]
	Optionally enable or disable sending any device events.

	The options available are:
	- *yes* - Events are sent as usual
	- *no* - No events are sent from this device
	- *disabledOnExternalMouse* - This device does not send events if an
	  external mouse has been detected.

	It is possible to prevent events from a device in the config and then do
	a Reconfigure to temporarily enable / disable specific devices.

	By default, this setting is not configured.

*<libinput><device><calibrationMatrix>* [six float values split by space]
	Apply the 3x3 transformation matrix to absolute device coordinates. This
	matrix has no effect on relative events.

	Given a 6-element array [a b c d e f], the matrix is applied as
```
    [ a  b  c ]   [ x ]
    [ d  e  f ] * [ y ]
    [ 0  0  1 ]   [ 1 ]

The most common matrices are:
    90 deg cw:      180 deg cw:     270 deg cw:     reflect along y axis:
    [ 0 -1 1]       [ -1  0 1]      [  0 1 0 ]      [ -1 0 1 ]
    [ 1  0 0]       [  0 -1 1]      [ -1 0 1 ]      [  1 0 0 ]
    [ 0  0 1]       [  0  0 1]      [  0 0 1 ]      [  0 0 1 ]
```

	visit https://wayland.freedesktop.org/libinput/doc/latest/absolute-axes.html#calibration-of-absolute-devices
	for more information.

*<libinput><scrollFactor>*
	Set scroll factor. Default is 1.0.

## WINDOW RULES

Two types of window rules are supported, actions and properties. They are
defined as shown below.

```
<windowRules>

  <!-- Action -->
  <windowRule identifier="" title="" type="">
    <action name=""/>
  </windowRule>

  <!-- Property -->
  <windowRule identifier="" serverDecoration="" />

</windowRules>
```

*Criteria*

*<windowRules><windowRule identifier="" title="" sandboxEngine=""
sandboxAppId="" type="" matchOnce="">*
	Define a window rule for any window which matches the criteria defined
	by the attributes *identifier*, *title*, or *type*. If more than one
	is defined, AND logic is used, so all have to match.
	Matching against patterns with '\*' (wildcard) and '?' (joker) is
	supported. Pattern matching is case-insensitive.

	*identifier* relates to app_id for native Wayland windows and WM_CLASS
	for XWayland clients.

	*title* is the title of the window.

	*sandboxEngine* is a sandbox engine name from the security context.

	*sandboxAppId* is a sandbox-specific identifier for an application
	from the security context.

	*type* [desktop|dock|toolbar|menu|utility|splash|dialog|dropdown_menu|
	popup_menu|tooltip|notification|combo|dnd|normal] relates to
	NET_WM_WINDOW_TYPE for XWayland clients. Native wayland clients have
	type "dialog" when they have a parent or a fixed size, or "normal"
	otherwise.

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
	*ignoreFocusRequest* prevents window to activate itself.

*<windowRules><windowRule ignoreConfigureRequest="">* [yes|no|default]
	*ignoreConfigureRequest* prevents a X11 window to position and size
	itself.

*<windowRules><windowRule fixedPosition="">* [yes|no|default]
	*fixedPosition* disallows interactive move/resize and prevents
	re-positioning in response to changes in reserved output space, which
	can be caused by *<margin>* settings or exclusive layer-shell clients
	such as panels.

## MENU

```
<menu>
  <ignoreButtonReleasePeriod>250</ignoreButtonReleasePeriod>
</menu>
```

*<menu><ignoreButtonReleasePeriod>*
	How long (in milliseconds) the initial button release event is ignored
	for. The reason for this logic and behaviour is to avoid a fast
	press-move-release sequence intended to just open the menu resulting in
	the closure of the menu or the selection of (typically the first) menu
	item. This behaviour only affects the first button-release. It is not
	anticipated that most users will want to change this, but the config
	option has been exposed for unusual use-cases. It is equivalent to
	Openbox's `<hideDelay>`. Default is 250 ms.

## MAGNIFIER

```
<magnifier>
  <width>400</width>
  <height>400</height>
  <initScale>2</initScale>
  <increment>0.2</increment>
  <useFilter>true</useFilter>
</magnifier>
```

*<magnifier><width>*
	Width of magnifier window in pixels. Default is 400.
	Set to -1 to use fullscreen magnifier.

*<magnifier><height>*
	Height of magnifier window in pixels. Default is 400.
	Set to -1 to use fullscreen magnifier.

*<magnifier><initScale>*
	Initial number of times by which magnified image is scaled. Value
	is the default at boot; can be modified at run-time in a keyboard
	or mouse binding by calling 'ZoomIn' or 'ZoomOut'. Default is x2.0.

*<magnifier><increment>*
	Step by which magnification changes on each call to 'ZoomIn' or
	'ZoomOut'. Default is 0.2.

*<magnifier><useFilter>* [yes|no|default]
	Whether to apply a bilinear filter to the magnified image, or
	just to use nearest-neighbour. Default is true - bilinear filtered.

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
