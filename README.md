# labwc

Aiming to become a light-weight openbox alternative for Wayland

## Dependencies

- wlroots (>=0.10.0)
- wayland-protocols

## Background

I am looking for a Wayland compositor that feels like openbox, but haven't come across one yet. Playing around with some code-bases seems an obvious way to evaluate and explore options. I saw [sway](https://swaywm.org/), [cage](https://www.hjdskes.nl/blog/cage-01/) and [wio](https://wio-project.org/), and definitely like the feel of wlroots.

Before trying wlroots, I messed around with [QtWayland](https://github.com/qt/qtwayland), [grefsen](https://github.com/ec1oud/grefsen), [Mir](https://mir-server.io) / [egmde](https://github.com/AlanGriffiths/egmde). Lubuntu have [declared](https://lubuntu.me/lubuntu-development-newsletter-9/) that they will be switching to Wayland by default for 20.10 and that they are going to do this by porting Openbox to use the Mir display server and [Drew DeVault’s](https://drewdevault.com/) QtLayerShell, etc.

Influenced by: [sway](https://github.com/swaywm/sway), [rootston](), [openbox](https://github.com/danakj/openbox), [i3](https://github.com/i3/i3), [dwm](https://dwm.suckless.org)

## Keyboard shortcuts

```
Alt+Escape  Exit labwc
Alt+F2      Cycle between windows
Alt+F3      Launch dmenu
Alt+F12     Print all views (helpful if run from X11)
```

