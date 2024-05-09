labwc-menu(5)

# NAME

labwc - menu files

# DESCRIPTION

Static menus are built based on content of XML files located at
"~/.config/labwc" and equivalent XDG Base Directories.

# SYNTAX

The menu file must be entirely enclosed within <openbox_menu> and
</openbox_menu> tags.  Inside these tags, menus are specified as follows:

```
<menu id="">

  <!-- A menu entry with an action, for example to execute an application -->
  <item label="">
    <action></action>
  </item>

  <!-- A submenu defined elsewhere -->
  <menu id="" />

  <!-- Horizontal line -->
  <separator />

  <!-- An inline submenu -->
  <menu id="" label="">
    ...some content...
  </menu>

  <!-- Title -->
  <separator label=""/>

  <!-- Pipemenu -->
  <menu id="" label="" execute="COMMAND"/>

</menu>
```

*menu.id* (when at toplevel)
	Define a menu tree. Each menu must be given an id, which is a unique
	identifier of the menu. This id is used to refer to the menu in a
	ShowMenu action. Default identifiers are
		- "root-menu" for the root window context menu
		- "client-menu" for a window's titlebar context menu

*menu.id* (when nested under other *<menu>* element)
	Link to a submenu defined elsewhere (by a *<menu id="">* at toplevel)

*menu.label*
	The title of the menu, shown in its parent. A label must be given when
	defining a menu.

*menu.item.label*
	The visible name of the menu item.

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


# LOCALISATION

Available localisation for the default "client-menu" is only shown if no
"client-menu" is present in menu.xml. Any menu definition in menu.xml is
interpreted as a user-override.

# SEE ALSO

labwc(1), labwc-actions(5), labwc-config(5), labwc-theme(5)
