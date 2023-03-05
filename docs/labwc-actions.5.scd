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

*<action name="Execute" command="value" />*
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

*<action name="MoveToEdge" direction="value" />*
	Move window to edge of outputs. Understands directions "left", "up",
	"right" and "down".

*<action name="Resize">*
	Begin interactive resize of window under cursor

*<action name="SnapToEdge" direction="value" />*
	Resize window to fill half the output in the given direction. Supports
	directions "left", "up", "right", "down" and "center".

*<action name="SnapToRegion" region="value" />*
	Resize and move active window according to the given region.
	See labwc-config(5) for further information on how to define regions.

*<action name="NextWindow">*
	Cycle focus to next window.

*<action name="PreviousWindow">*
	Cycle focus to previous window.

*<action name="Reconfigure">*
	Re-load configuration and theme files.

*<action name="ShowMenu" menu="value" />*
	Show menu. Valid menu names are "root-menu" and "client-menu".

*<action name="ToggleDecorations">*
	Toggle decorations of focused window.

*<action name="ToggleFullscreen">*
	Toggle fullscreen state of focused window.

*<action name="ToggleMaximize">*
	Toggle maximize state of focused window.

*<action name="ToggleAlwaysOnTop">*
	Toggle always-on-top of focused window.

*<action name="ToggleKeybinds">*
	Stop handling keybinds other than ToggleKeybinds itself.
	This can be used to allow A-Tab and similar keybinds to be delivered
	to Virtual Machines, VNC clients or nested compositors.
	A second call will restore all original keybinds.

*<action name="FocusOutput" output="HDMI-A-1" />*
	Give focus to topmost window on given output and warp the cursor
	to the center of the window. If the given output does not contain
	any windows, the cursor is centered on the given output.

*<action name="GoToDesktop" to="value" />*
	Switch to workspace. Supported values are "last", "left", "right" or the
	full name of a workspace or its index (starting at 1) as configured in
	rc.xml.

*<action name="SendToDesktop" to="value" />*
	Send active window to workspace.
	Supported values are the same as for GoToDesktop.

*<action name="None">*
	If used as the only action for a binding: clear an earlier defined binding.

# SEE ALSO

labwc(1), labwc-config(5), labwc-theme(5)
