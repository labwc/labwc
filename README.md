# labwc

A light-weight openbox alternative for Wayland.

This software is in early development.

# Dependencies

- wlroots (>=0.10.0)
- wayland-protocols

# Aim

[x] Support xwayland
[ ] Support some of openbox's rc.xml
[ ] Support openbox themes
[ ] Support layer-shell's background layer

# Influenced by

- [sway](https://github.com/swaywm/sway)
- [cage](https://www.hjdskes.nl/blog/cage-01/)
- [wio](https://wio-project.org/)
- [rootston]()
- [openbox](https://github.com/danakj/openbox)
- [i3](https://github.com/i3/i3)
- [dwm](https://dwm.suckless.org)

# Alternatives

The following were considered before choosing wlroots:

- [QtWayland](https://github.com/qt/qtwayland), [grefsen](https://github.com/ec1oud/grefsen)
- [Mir](https://mir-server.io), [egmde](https://github.com/AlanGriffiths/egmde)

# CONFIGURATION

## Keyboard shortcuts

We will support rc.xml keybinds, but for the time being:

```
Alt+Escape  Exit labwc
Alt+F2      Cycle windows
Alt+F3      Launch dmenu
```

## Keyboard layout

Set environment variable `XKB_DEFAULT_LAYOUT` for your keyboard layout, for
example `gb`. Read `xkeyboard-config(7)` for details.

# Integration

- Use grim for scrots

