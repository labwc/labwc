% labwc(5)
% Johan Malm
% 22 July, 2020

# NAME

labwc - A Wayland stacking compositor with the look and feel of Openbox

# CONFIGURATION

Configuration aims to be compatible with Openbox. Where there are differences,
these are pointed out.

## rc.xml

### rc.xml lab section

The `<lab>` stanza contains some labwc specific settings which are not present in Openbox.

    <lab>
      <csd></csd>
      <keyboard>
        <layout></layout>
      </keyboard>
    </lab>

**csd** Use client-side decorations for xdg-shell views.

**keyboard-layout** Set `XKB_DEFAULT_LAYOUT`. See xkeyboard-config(7) for details.

### rc.xml theme section

    <theme>
      <name></name>
    </theme>

**name** The name of the Openbox theme to use

### rc.xml keyboard section

This section describes key bindings.

    <keyboard>
      <keybind key="KEY-COMINATION">
        ACTION
      </keybind>
    <keyboard>

**KEY-COMINATION** The key combination to bind to an action in the format **modifier**-**key**.

Supported **modifiers** include:

- S Shift
- C Control
- A Alt
- W Super key

Unlike Openbox, multiple space-separated **KEY-COMINATION** and key-chains are not supported.

Example:

    <keyboard>
      <keybind key="A-Escape">
        <action name="Exit"/>
      </keybind>
      <keybind key="A-Tab">
        <action name="NextWindow"/>
      </keybind>
      <keybind key="A-F3">
        <action name="Execute">
          <command>dmenu_run</command>
        </action>
      </keybind>
    <keyboard>

## Actions

Actions are used in key bindings.

Action syntax:

    <action name="NAME">
      OPTION
    </action>

**NAME** is the name of the action as listed below.

**OPTION** is a set of tags specific to each action as defined below.

### Action Execute

Execute command specified by `<command>` option.

### Action Exit

Exit labwc.

### Action NextWindow

Cycle focus to next window.

## themerc

**window.active.title.bg.color** Specify the background for the focussed window's titlebar



**window.active.handle.bg.color** Specify the background for the focussed window's handle.


**window.inactive.title.bg.color** Specify the background for non-focussed windows' titlebars

# DEFINITIONS

The **handle** is the window decoration placed on the bottom of the window.

# EXAMPLES

## Example 1 - title bar configuration

    +-----------------------------------------+ ^
    |                                         | |
    |                                         | |
    |                                         | | h
    |                                         | |
    +-----------------------------------------+ v

    h = padding * 2 + font vertical extents


# SEE ALSO

labwc(1)
