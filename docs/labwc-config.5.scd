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
	If no place attribute is provided, the setting will be applied to all
	places.

*<theme><font place=""><name>*
	Describes font name. Default is sans.

*<theme><font place=""><size>*
	Font size in pixels. Default is 8.

# KEYBOARD

*<keyboard><keybind key="">*
	Define a key binding in the format *modifier-key*, where supported
	modifiers include S (shift); C (control); A (alt); W (super). Unlike
	Openbox, multiple space-separated key combinations and key-chains are
	not supported.

*<keyboard><keybind key=""><action name="">*
	Keybind action. See labwc-action(5)

Default key-binds if no rc.xml is found:

```
<keyboard>
  <keybind key="A-Escape">
    <action name="Exit"/>
  </keybind>
  <keybind key="A-Tab">
    <action name="NextWindow"/>
  </keybind>
  <keybind key="A-F3">
    <action name="Execute">
      <command>bemenu-run</command>
    </action>
  </keybind>
<keyboard>
```

# SEE ALSO

labwc(1), labwc-actions(5), labwc-theme(5)
