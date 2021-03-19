labwc-menu(5)

# NAME

labwc - menu

# DESCRIPTION

Static menus are built based on content of XML files located at
"~/.config/openbox" and equivalent XDG Base Directories.

# SYNTAX

A menu file must be entirely enclosed within <openbox_menu> and
</openbox_menu> tags.  Inside these tags, menus are specified as follows:

```
<menu id="">
  <item label="">
    <action></action>
  </item>
</menu>
```

*menu.id*
	Each menu must be given an id, which is a unique identifier of the menu.
	This id is used to refer to the menu in a ShowMenu action.

*menu.item.label*
	The visible name of the menu item.

*menu.item.action*
	See labwc-action(5)

# SEE ALSO

labwc(1), labwc-action(5), labwc-config(5), labwc-theme(5)
