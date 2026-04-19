labmsg(1)

# NAME

labmsg - IPC client for labwc

# SYNOPSIS

_labmsg_ [options...] [message]

# DESCRIPTION

labmsg is a command-line IPC client for *labwc*(1). It communicates with a
running labwc instance over a UNIX domain socket using a swaymsg-compatible
binary protocol.

The socket path is obtained from the *LABWC_IPC_SOCK* environment variable,
which labwc sets automatically for child processes. It can also be specified
explicitly with the *-s* option.

# OPTIONS

*-h, --help*
	Show help message and quit.

*-m, --monitor*
	After sending the initial message, remain connected and continuously
	print events received from the compositor. Typically used with
	*-t subscribe*.

*-p, --pretty*
	Force pretty-printed (indented) JSON output.

*-q, --quiet*
	Suppress response output. The exit code still reflects success or
	failure.

*-r, --raw*
	Force compact (single-line) JSON output. When neither *-p* nor *-r* is
	given, labmsg auto-detects: pretty if stdout is a terminal, raw
	otherwise.

*-s, --socket* <path>
	Use _path_ as the IPC socket instead of *LABWC_IPC_SOCK*.

*-t, --type* <type>
	Set the IPC message type. The default is *run_command*. See *MESSAGE
	TYPES* below for the full list.

*-v, --version*
	Show the version number and quit.

# MESSAGE TYPES

The following message types are supported via *-t*:

*run_command*
	Execute one or more compositor commands (the default). Commands are
	passed as the trailing message argument. Multiple commands can be
	separated by semicolons. See *COMMANDS* below.

*get_workspaces*
	Return a JSON array of workspace objects, each containing _num_, _name_,
	_visible_, _focused_, _urgent_, _output_, and _rect_ fields.

*subscribe*
	Subscribe to compositor events. The message payload must be a JSON array
	of event type names. See *EVENTS* below. Typically combined with *-m*.

*get_outputs*
	Return a JSON array of output objects with properties such as _name_,
	_make_, _model_, _serial_, _active_, _scale_, _transform_,
	_current_workspace_, _modes_, _current_mode_, and _rect_.

*get_tree*
	Return the full window tree as a nested JSON object. The root contains
	output nodes, each containing workspace nodes, each containing
	floating view nodes.

*get_bar_config*
	Return bar configuration. Since labwc has no built-in bar, this returns
	an empty array (or empty object if a bar ID is given).

*get_version*
	Return a JSON object with _major_, _minor_, _patch_,
	_human_readable_, and _loaded_config_file_name_ fields.

*get_config*
	Return a JSON object with a _config_ field containing the raw text of
	the currently loaded rc.xml configuration file.

*send_tick*
	Broadcast a tick event to all subscribers. The message payload is
	forwarded as the tick _payload_ string.

*get_inputs*
	Return a JSON array of input device objects with _identifier_, _name_,
	_vendor_, _product_, _type_, and device-specific fields such as
	_xkb_active_layout_name_ for keyboards or _scroll_factor_ for
	pointers.

*get_seats*
	Return a JSON array of seat objects with _name_, _capabilities_,
	_focus_, and a _devices_ array.

# COMMANDS

When the message type is *run_command* (the default), the trailing arguments
form a command string. Multiple commands can be separated by semicolons (*;*).

Commands may be prefixed with a criteria block to target specific windows:

	\[app_id="<pattern>" title="<pattern>" class="<pattern>"\]

If no criteria are given, the command operates on the currently focused window
where applicable.

The following commands are supported:

*nop*
	No operation.

*exit*
	Exit the compositor.

*reload*
	Reload the compositor configuration.

*exec* [--no-startup-id] _<command>_
	Execute _command_ via the shell.

*kill*
	Close the target window.

*fullscreen* [toggle]
	Toggle fullscreen mode on the target window.

*floating toggle*
	If the target window is tiled, untile it. Otherwise no-op (labwc is a
	stacking compositor).

*sticky toggle*
	Toggle the target window's omnipresent (visible on all workspaces) state.

*border* none|normal|pixel|toggle
	Set or toggle the target window's border decoration mode.

*workspace* _<name>_
	Switch to the named workspace. The special names *next*, *prev*,
	*next_on_output*, and *prev_on_output* cycle through workspaces.

*focus output* _<name>_|left|right|up|down
	Focus the given output by name or direction.

*move position* _<x>_ _<y>_
	Move the target window to absolute coordinates.

*move* [container|window] [to] workspace _<name>_
	Move the target window to the named workspace.

*move* [container|window] [to] output _<name>_|left|right|up|down
	Move the target window to the given output.

*move* left|right|up|down [_<pixels>_]
	Move the target window by _pixels_ in the given direction (default 10).

*resize set* _<width>_ _<height>_
	Resize the target window to exact dimensions.

*resize* grow|shrink width|height _<amount>_ [px|ppt]
	Resize the target window incrementally (default amount 10).

# EVENTS

When using *-t subscribe*, the message payload must be a JSON array of event
type names. For example:

```
labmsg -t subscribe -m '["workspace", "window"]'
```

The following event types are available:

*workspace*
	Emitted on workspace changes. The event object contains _change_
	(*"focus"*, *"init"*, etc.), _current_, and _old_ fields.

*output*
	Emitted on output changes. Contains a _change_ field.

*window*
	Emitted on window changes. Contains _change_ (*"new"*, *"close"*,
	*"focus"*, *"title"*, *"fullscreen_mode"*, *"move"*, *"resize"*) and a
	_container_ field with the view properties.

*shutdown*
	Emitted when the compositor is about to exit. Contains _change_
	(*"exit"*).

*tick*
	Emitted when a *send_tick* message is received. Contains _first_ (boolean,
	true for the initial tick on subscription) and _payload_ fields.

# ENVIRONMENT

*LABWC_IPC_SOCK*
	Path to the labwc IPC UNIX domain socket. Set automatically by labwc
	for child processes. Overridden by *-s*.

# EXAMPLES

Query the current workspaces:

```
labmsg -t get_workspaces
```

Get the window tree in compact form:

```
labmsg -r -t get_tree
```

Close the focused window:

```
labmsg kill
```

Move a specific window to workspace 2:

```
labmsg '[app_id="firefox"] move container to workspace 2'
```

Subscribe to window events:

```
labmsg -t subscribe -m '["window"]'
```

Execute a command:

```
labmsg exec foot
```

Chain multiple commands:

```
labmsg 'workspace 3; exec firefox'
```

# SEE ALSO

labwc(1), labwc-actions(5)
