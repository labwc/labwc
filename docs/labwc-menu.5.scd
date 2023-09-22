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

  <!-- Horizontal line >
  <separator />

  <!-- An inline submenu -->
  <menu id="" label="">
    ...some content...
  </menu>

</menu>
```

*menu.id*
	Each menu must be given an id, which is a unique identifier of the menu.
	This id is used to refer to the menu in a ShowMenu action. 
	Default identifiers are "client-menu" for the titlebar context menu and "root-menu" 
	for the root window context menu. 
	Available localisation for the default "client-menu" is
	only shown if no "client-menu" is present in menu.xml.

*menu.label*
	The title of the menu, shown in its parent. A label must be given when
	defining a menu.

*menu.item.label*
	The visible name of the menu item.

*menu.item.action*
	See labwc-action(5). Note: XML CDATA is supported for this node in
	order to maintain compatibility with obmenu-generator.

*menu.separator*
	Horizontal line.

# SEE ALSO

labwc(1), labwc-action(5), labwc-config(5), labwc-theme(5)
