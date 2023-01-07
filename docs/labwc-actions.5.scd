labwc-actions(5)

# NAME

labwc - actions

# ACTIONS

Actions are used in menus and keyboard/mouse bindings.

*<action name="Close">*
	Close top-most window.

*<action name="Kill">*
	Kill the process associated with the current window by sending it the
	SIGTERM signal.

*<action name="Execute"><command>*
	Execute command.  Note that in the interest of backward compatibility,
	labwc supports <execute> as an alternative to <command> even though
	openbox documentation states that it is deprecated.

*<action name="Exit">*
	Exit labwc.

*<action name="Focus">*
	Give focus to window under cursor.

*<action name="Raise">*
	Restack the current window above other open windows.

*<action name="Iconify">*
	Iconify (minimize) focused window.

*<action name="Move">*
	Begin interactive move of window under cursor

*<action name="MoveToEdge"><direction>*
	Move window to edge of outputs. Understands directions "left", "up",
	"right" and "down".

*<action name="Resize">*
	Begin interactive resize of window under cursor

*<action name="SnapToEdge"><direction>*
	Resize window to fill half the output in the given direction. Supports
	directions "left", "up", "right", "down" and "center".

*<action name="SnapToRegion" region="snap-1" />*
	Resize and move active window according to the given region.
	See labwc-config(5) for further information on how to define regions.

*<action name="NextWindow">*
	Cycle focus to next window.

*<action name="PreviousWindow">*
	Cycle focus to previous window.

*<action name="Reconfigure">*
	Re-load configuration and theme files.

*<action name="ShowMenu"><menu>*
	Show menu. Valid menu names are "root-menu" and "client-menu".

*<action name="ToggleDecorations">*
	Toggle decorations of focused window.

*<action name="ToggleFullscreen">*
	Toggle fullscreen state of focused window.

*<action name="ToggleMaximize">*
	Toggle maximize state of focused window.

*<action name="ToggleAlwaysOnTop">*
	Toggle always-on-top of focused window.

*<action name="GoToDesktop"><to>*
	Switch to workspace. Supported values are "last", "left", "right" or the
	full name of a workspace or its index (starting at 1) as configured in
	rc.xml.

*<action name="SendToDesktop"><to>*
	Send active window to workspace.
	Supported values are the same as for GoToDesktop.

*<action name="None">*
	If used as the only action for a binding: clear an earlier defined binding.

# SEE ALSO

labwc(1), labwc-config(5), labwc-theme(5)
