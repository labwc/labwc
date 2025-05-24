labwc-menu(5)

# NAME

labwc - menu file

# DESCRIPTION

Static menus are built based on the menu.xml file located at
"~/.config/labwc" and equivalent XDG Base Directories.

# SYNTAX

The menu file must be entirely enclosed within <openbox_menu> and
</openbox_menu> tags.  Inside these tags, menus are specified as follows:

```
<!-- A toplevel menu -->
<menu id="external_menu_settings" label="External Settings" icon="preferences-other">
  <item label="Display setting">
    <action name="Execute" command="wdisplays" />
  </item>
</menu>

<!-- Another toplevel menu -->
<menu id="">

  <!-- A menu entry with an action, for example to execute an application -->
  <item label="" icon="">
    <action></action>
  </item>

  <!-- A submenu defined elsewhere, uses external label and icon attributes -->
  <menu id="external_menu_settings" />

  <!-- Horizontal line -->
  <separator />

  <!-- An inline submenu -->
  <menu id="" label="" icon="">
    ...some content...
  </menu>

  <!-- Title -->
  <separator label="" />

  <!-- Pipemenu -->
  <menu id="" label="" icon="" execute="COMMAND" />

</menu>
```

*menu.id* (when at toplevel)
	Define a menu tree. Each menu must be given an id, which is a unique
	identifier of the menu. This id is used to refer to the menu in a
	ShowMenu action. Default identifiers are
		- "root-menu" for the root window context menu
		- "client-menu" for a window's titlebar context menu
		- "client-list-combined-menu" for a list of all windows across
		  all workspaces. Will change focus to the app that gets
		  selected or go to workspace without activating any app if
		  "Go there" is selected.
		- "client-send-to-menu" shows all workspaces and sends current
		  window to that workspace when selected.

*menu.id* (when nested under other *<menu>* element)
	Link to a submenu defined elsewhere (by a *<menu id="">* at toplevel).

*menu.label*
	The title of the menu, shown in its parent. A label must be given when
	defining a menu unless it is the topmost menu element.

	Note that Openbox requires a label="" defined for topmost elements too,
	but for simplicity labwc handles these with or without the label
	attribute.

*menu.icon*
	An icon to be rendered, shown in its parent. This argument is optional.
	See *menu.item.icon* for further details.

*menu.item.label*
	The visible name of the menu item.

*menu.item.icon*
	The name of an icon to be rendered in front of the menu entry. This
	attribute is optional. The name follows naming conventions of "Icon="
	entries in .desktop files. If used, it is recommended to use the name of
	the icon only rather than a full path. This ensures that the icon will
	be looked up in the scale of the output where the menu is rendered. E.g.
	use of icon="vlc" is suggested over using
	icon="/usr/share/icons/hicolor/16x16/apps/vlc.png".

*menu.item.action*
	See labwc-actions(5). Note: XML CDATA is supported for this node in
	order to maintain compatibility with obmenu-generator.

*menu.separator*
	Horizontal line.

*menu.separator.label*
	In a "separator" element, the label attribute transforms the separator
	from a horizontal line to a menu title (heading) with the text specified
	by label in it.

*menu.execute*
	Command to execute for pipe menu. See details below.

# PIPE MENUS

Pipe menus are menus generated dynamically based on output of scripts or
binaries. They are so-called because the output of the executable is piped to
the labwc menu.

For any *<menu id="" label="" execute="COMMAND"/>* entry in menu.xml, the
COMMAND will be executed the first time the item is selected (for example by
cursor or keyboard input). The XML output of the command will be parsed and
shown as a submenu. The content of pipemenus is cached until the whole menu
(not just the pipemenu) is closed.

The content of the output must be entirely enclosed within *<openbox_pipe_menu>*
tags. Inside these, menus are specified in the same way as static (normal)
menus, for example:

```
<openbox_pipe_menu>
  <item label="Terminal">
    <action name="Execute" command="xterm"/>
  </item>
</openbox_pipe_menu>
```

Inline submenus and nested pipemenus are supported.

Note that it is the responsibility of the pipemenu executable to ensure that
ID attributes are unique. Duplicates are ignored.

When writing pipe menu scripts, make sure to escape XML special characters such
as "&" ("&amp;"), "<" ("&lt;"), and ">" ("&gt;").

A pipemenu can also be used to define the toplevel *<menu>* element. In this
case the entire menu.xml file would be reduced to something like this (replacing
obmenu-generator with the menu generator of your choice):

```
<?xml version="1.0"?>
<openbox_menu>
  <menu id="root-menu" label="" execute="obmenu-generator"/>
</openbox_menu>
```

# LOCALISATION

Available localisation for the default "client-menu" is only shown if no
"client-menu" is present in menu.xml. Any menu definition in menu.xml is
interpreted as a user-override.

# SEE ALSO

labwc(1), labwc-actions(5), labwc-config(5), labwc-theme(5)
