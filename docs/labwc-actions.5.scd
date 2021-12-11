labwc-actions(5)

# NAME

labwc - actions

# ACTIONS

Actions are used in keyboard bindings.

*<action name="Close"><command>*
	Close top-most view.

*<action name="Execute"><command>*
	Execute command.

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
	Move view to edge of outputs. Understands directions "left", "up",
	"right" and "down".

*<action name="Resize">*
	Begin interactive resize of window under cursor

*<action name="SnapToEdge"><direction>*
	Resize view to fill half the output in the given direction. Supports
	directions "left", "up", "right", "down" and "center".

*<action name="NextWindow">*
	Cycle focus to next window.

*<action name="PreviousWindow">*
	Cycle focus to previous window.

*<action name="Reconfigure">*
	Re-load configuration and theme files.

*<action name="ShowMenu"><menu>*
	Show menu. Valid menu name is "root-menu".

*<action name="ToggleDecorations">*
	Toggle decorations of focused window.

*<action name="ToggleFullscreen">*
	Toggle fullscreen state of focused window.

*<action name="ToggleMaximize">*
	Toggle maximize state of focused window.

# SEE ALSO

labwc(1), labwc-config(5), labwc-theme(5)
