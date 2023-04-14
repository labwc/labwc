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

*<action name="Exit" />*
	Exit labwc.

*<action name="Focus" />*
	Give focus to window under cursor.

*<action name="Raise" />*
	Restack the current window above other open windows.

*<action name="Lower" />*
	Restack the current window below other open windows.

*<action name="Iconify" />*
	Iconify (minimize) focused window.

*<action name="Move" />*
	Begin interactive move of window under cursor

*<action name="MoveToEdge" direction="value" />*
	Move window to edge of outputs. Understands directions "left", "up",
	"right" and "down".

*<action name="Resize" />*
	Begin interactive resize of window under cursor

*<action name="ResizeRelative" left="" right="" top="" bottom="" />*
	Resize window relative to its current size. Values of left, right,
	top or bottom tell how much to resize on that edge of window,
	positive values grow window, negative shrink window.

*<action name="MoveTo" x="" y="" />*
	Move to position (x, y)

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

*<action name="ShowMenu" menu="value" />*
	Show menu. Valid menu names are "root-menu" and "client-menu".

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

*<action name="ToggleMaximize" />*
	Toggle maximize state of focused window.

*<action name="Maximize" />*
	Maximize focused window.

*<action name="ToggleAlwaysOnTop" />*
	Toggle always-on-top of focused window.

*<action name="ToggleAlwaysOnBottom" />*
	Toggle between layers 'always-on-bottom' and 'normal'. When a window is
	in the 'always-on-bottom' layer, it is rendered below all other
	top-level windows. It is anticipated that this action will be useful
	when defining window-rules for desktop-management tools that do not
	support the wlr-layer-shell protocol.

*<action name="ToggleKeybinds" />*
	Stop handling keybinds other than ToggleKeybinds itself.
	This can be used to allow A-Tab and similar keybinds to be delivered
	to Virtual Machines, VNC clients or nested compositors.
	A second call will restore all original keybinds.

	This action will only affect the window that had keyboard focus when
	the binding was executed. Thus when switching to another window, all
	the usual keybinds will function again until switching back to the
	original window. There can be multiple windows with this mode set.

*<action name="FocusOutput" output="HDMI-A-1" />*
	Give focus to topmost window on given output and warp the cursor
	to the center of the window. If the given output does not contain
	any windows, the cursor is centered on the given output.

*<action name="GoToDesktop" to="value" wrap="yes" />*
	Switch to workspace.

	*to* The workspace to switch to. Supported values are "last", "left",
	"right" or the full name of a workspace or its index (starting at 1)
	as configured in rc.xml.

	*wrap* [yes|no] Wrap around from last desktop to first, and vice
	versa. Default yes.

*<action name="SendToDesktop" to="value" follow="yes" wrap="yes" />*
	Send active window to workspace.

	*to* The workspace to send the window to. Supported values are the same
	as for GoToDesktop.

	*follow* [yes|no] Also switch to the specified workspace. Default yes.

	*wrap* [yes|no] Wrap around from last desktop to first, and vice
	versa. Default yes.

*<action name="None" />*
	If used as the only action for a binding: clear an earlier defined binding.

# SEE ALSO

labwc(1), labwc-config(5), labwc-theme(5)
