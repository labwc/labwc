labwc-actions(5)

# NAME

labwc - actions

# ACTIONS

Actions are used in menus and keyboard/mouse bindings.

*<action name="Close" />*
	Close top-most window.

*<action name="Kill" />*
	Kill the process associated with the current window by sending it the
	SIGTERM signal.

*<action name="Execute" command="value" />*
	Execute command.  Note that in the interest of backward compatibility,
	labwc supports <execute> as an alternative to <command> even though
	openbox documentation states that it is deprecated.
	Note: Tilde (~) is expanded in the command before passing to execvp().

*<action name="Exit" />*
	Exit labwc.

*<action name="Focus" />*
	Give focus to window under cursor.

*<action name="Unfocus" />*
	Remove focus from the window that is currently focused.

*<action name="Raise" />*
	Restack the current window above other open windows.

*<action name="Lower" />*
	Restack the current window below other open windows.

*<action name="Iconify" />*
	Iconify (minimize) focused window.

*<action name="Move" />*
	Begin interactive move of window under cursor.

*<action name="MoveToEdge" direction="value" snapWindows="value" />*
	Move window until it hits the next edge.

	*direction* [left|up|right|down] Direction in which to move.

	*snapWindows* [yes|no] Move window until it hits an edge of
	another window or screen edge. If set to "no", only move to
	the next screen edge. Default is yes.

*<action name="Resize" />*
	Begin interactive resize of window under cursor.

*<action name="ResizeRelative" left="" right="" top="" bottom="" />*
	Resize window relative to its current size. Values of left, right,
	top or bottom tell how much to resize on that edge of window,
	positive values grow window, negative shrink window.

*<action name="GrowToEdge" direction="value" />*
	Resize window to fill the space between its edge and any other
	window edge.

	*direction* [left|up|right|down] Direction in which to grow.

*<action name="ShrinkToEdge" direction="value" />*
	Reverse of GrowToEdge. Shrinks by a maximum of 50%.

	*direction* [left|up|right|down] Direction in which to shrink.

*<action name="MoveTo" x="" y="" />*
	Move to position (x, y).

*<action name="ResizeTo" width="" height="" />*
	Resize window.

	*width* The width to resize the window to in pixels.

	*height* The height to resize the window to in pixels.

*<action name="MoveToCursor" />*
	Move to be centered on cursor.
	Tries to prevent any part of the window from going off-screen.
	This action is deprecated from v0.7.3. To ensure your config works in
	future labwc releases, please use:
	*<action name="AutoPlace" policy="cursor">*

*<action name="MoveRelative" x="" y="" />*
	Move window relative to its current position. Positive value of x moves
	it right, negative left. Positive value of y moves it down, negative up.

*<action name="SnapToEdge" direction="value" />*
	Resize window to fill half the output in the given direction. Supports
	directions "left", "up", "right", "down" and "center".

*<action name="SnapToRegion" region="value" />*
	Resize and move active window according to the given region.
	See labwc-config(5) for further information on how to define regions.

*<action name="NextWindow" />*
	Cycle focus to next window.

*<action name="PreviousWindow" />*
	Cycle focus to previous window.

*<action name="Reconfigure" />*
	Re-load configuration and theme files.

*<action name="ShowMenu" menu="value" atCursor="yes" />*
	Show a menu.

	*menu* The name of the menu to show. The menus "root-menu" and
	"client-menu" are guaranteed to exist, but others may be defined
	explicitly. See labwc-menu(5) for more information.

	*atCursor* [yes|no] When opening a menu, open the menu at the location
	of the mouse cursor. When set to no, the menu will appear at the
	upper-left corner of the window associated with the action. Default is
	yes.

*<action name="ToggleDecorations" />*
	Toggle decorations of focused window.

	This is a 3-state action which can be executed multiple times:
	- Only the titlebar will be hidden, borders and resize area are kept
	- Remaining decorations will be disabled
	- Decorations will be shown normally

	By disabling the theme configuration 'keepBorder' the first step
	will be removed and the action only toggles between on and off.

*<action name="ToggleFullscreen" />*
	Toggle fullscreen state of focused window.

*<action name="ToggleMaximize" direction="value" />*
	Toggle maximize state of focused window. Supported directions are
	"both" (default), "horizontal", and "vertical".

*<action name="Maximize" direction="value" />*
	Maximize focused window. Supported directions are "both" (default),
	"horizontal", and "vertical".

*<action name="ToggleAlwaysOnTop" />*
	Toggle always-on-top of focused window.

*<action name="ToggleAlwaysOnBottom" />*
	Toggle between layers 'always-on-bottom' and 'normal'. When a window is
	in the 'always-on-bottom' layer, it is rendered below all other
	top-level windows. It is anticipated that this action will be useful
	when defining window-rules for desktop-management tools that do not
	support the wlr-layer-shell protocol.

*<action name="ToggleOmnipresent" />*
	Toggle omnipresent (visible on all workspaces / sticky) for the focused
	window.

*<action name="ToggleKeybinds" />*
	Stop handling keybinds other than ToggleKeybinds itself.
	This can be used to allow A-Tab and similar keybinds to be delivered
	to Virtual Machines, VNC clients or nested compositors.
	A second call will restore all original keybinds.

	This action will only affect the window that had keyboard focus when
	the binding was executed. Thus when switching to another window, all
	the usual keybinds will function again until switching back to the
	original window. There can be multiple windows with this mode set.

*<action name="ToggleTearing" />*
	Toggles tearing for the focused window.

*<action name="FocusOutput" output="HDMI-A-1" />*
	Give focus to topmost window on given output and warp the cursor
	to the center of the window. If the given output does not contain
	any windows, the cursor is centered on the given output.

*<action name="MoveToOutput" output="HDMI-A-1" direction="value" wrap="no" />*
	Moves active window to other output, unless the window state is
	fullscreen.

	If *output* is specified, the window will be sent directly to the
	specified output and *direction* is ignored. If *output* is omitted,
	*direction* may be one of "left", "right", "up" or "down" to indicate
	that the window should be moved to the next output in that direction
	(if one exists).

	*wrap* [yes|no] When using the direction attribute, wrap around from
	right-to-left or top-to-bottom, and vice versa. Default no.

*<action name="FitToOutput" />*
	Resizes active window size to width and height of the output when the
	window size exceeds the output size.

*<action name="GoToDesktop" to="value" wrap="yes" />*
	Switch to workspace.

	*to* The workspace to switch to. Supported values are "current", "last",
	"left", "right" or the full name of a workspace or its index (starting
	at 1) as configured in rc.xml.

	*wrap* [yes|no] Wrap around from last desktop to first, and vice
	versa. Default yes.

*<action name="SendToDesktop" to="value" follow="yes" wrap="yes" />*
	Send active window to workspace.

	*to* The workspace to send the window to. Supported values are the same
	as for GoToDesktop.

	*follow* [yes|no] Also switch to the specified workspace. Default yes.

	*wrap* [yes|no] Wrap around from last desktop to first, and vice
	versa. Default yes.

*<action name="VirtualOutputAdd" output_name="value" />*
	Add virtual output (headless backend).

	For example, it can be used to overlay virtual output on real output,
	but with a different resolution (this can be done with `wlr-randr`
	or `wdisplays`). After that, virtual output can be selected for screen
	sharing (casting), effectively sharing only the region of the screen.

	It must be noted that overlaying virtual output and real output is not
	endorsed or explicitly supported by wlroots. For example, after
	configuring virtual output, real output must be reconfigured as well
	(for the overlay configuration to work correctly). This is the example
	configuration:

```
<keybind key="W-v">
  <action name="VirtualOutputAdd" output_name="ScreenCasting"/>
  <action name="Execute" command='sh -c "wlr-randr --output ScreenCasting --pos 0,0 --scale 2 --custom-mode 3840x2110; wlr-randr --output eDP-1 --pos 0,0 --scale 2 --mode 3840x2160"'/>
</keybind>
<keybind key="W-c">
  <action name="VirtualOutputRemove"/>
</keybind>
```

	Note that the vertical resolution of "ScreenCasting" output is just 50px
	smaller than "eDP-1" output to cut off bottom panel from screen sharing.

	Virtual output is also useful for extending the desktop to (maybe
	mobile) remote systems like tablets. E.g. simply adding a virtual
	output, attaching wayvnc to it and running a VNC client on the remote
	system.

	*output_name* The name of virtual output. Providing virtual output name
	is beneficial for further automation. Default is "HEADLESS-X".

*<action name="VirtualOutputRemove" output_name="value" />*
	Remove virtual output (headless backend).

	*output_name* The name of virtual output. If not supplied, will remove
	the last virtual output added.

*<action name="AutoPlace" />*
	Use the automatic placement policy to move the active window to a
	position on its output that will minimize overlap with other windows.

*<action name="Shade" />*++
*<action name="Unshade" />*++
*<action name="ToggleShade" />*
	Set, unset, or toggle, respectively, the "shaded" state of the active
	window. When shaded, window contents are hidden, leaving only the
	titlebar visible. Full-screen windows or those without server-side
	decorations (including those for which the server-side titlebar has been
	hidden) are not eligible for shading.

*<action name="ToggleMagnify">*
	Toggle the screen magnifier on or off at the last magnification level
	used.

*<action name="ZoomIn">*++
*<action name="ZoomOut">*
	Increase or decrease the magnification level for the screen magnifier.
	If the magnifier is currently off, ZoomIn will enable it at the lowest
	magnification, equal to (1 + the magnifier increment set in the theme).
	If the magnifier is on and at the lowest magnification, ZoomOut will
	turn it off.

*<action name="None" />*
	If used as the only action for a binding: clear an earlier defined
	binding.

# CONDITIONAL ACTIONS

Actions that execute other actions. Used in keyboard/mouse bindings.

*<action name="If">*
	This action will execute one set of actions if the focused window
	matches the criteria, or another if it does not.

	The arguments are as follows:

	```
	<action name="If">
	  <query/>
	  <then><action/></then>
	  <else><action/></else>
	</action>
	```

	*query*
		Define a query with zero or more conditions. All conditions must
		be evaluated as true in order for the window to match this
		query. Multiple queries can be defined.

		Pattern matching is done according to glob(7) and is
		case-insensitive.

		Conditions are as follows:

		*identifier*
			XDG shell app_id for Wayland clients, WM_CLASS for
			XWayland clients.

		*title*
			XDG shell title for Wayland clients, WM_NAME for
			XWayland clients.

		*type*
			Internal heuristics for Wayland clients,
			NET_WM_WINDOW_TYPE for XWayland clients.

		This argument is optional.

	*then*
		A list of actions to be executed if the window matches any
		query. This argument is optional.

	*else*
		A list of actions to be executed if the window does not match
		any query. This argument is optional.

*<action name="ForEach">*
	Identical to "If" action, but applies to all windows, not just the
	focused one.

	The *ForEach* action has another optional *none* branch which gets
	executed when no window has been matched by the query. This allows
	for example to implement a run-or-raise functionality.

# SEE ALSO

labwc(1), labwc-config(5), labwc-theme(5), glob(7)
