# labwc

labwc is a wayland compositor based on wlroots

## Dependencies

- wlroots
- wayland-protocols

## Keyboard shortcuts

```
Alt+Escape  Exit labwc
Alt+F2      Cycle between windows
Alt+F3      Launch dmenu
Alt+F6      Move window
Alt+F12     Print all views (helpful if run from X11)
```

## Running labwc

labwc can be run from a tty or in an existing Wayland/X11 session.

## Why?

I saw [sway](https://swaywm.org/), [cage](https://www.hjdskes.nl/blog/cage-01/) and [wio](https://wio-project.org/), and felt the itch to have a go at hacking on a wlroots compositor myself.

I am also quietly looking for a Wayland alternative to openbox and playing around with some code-bases seems an obvious way to evaluate and explore options.

<b>QtWayland and Mir</b>

Before trying wlroots, I messed around with [QtWayland](https://github.com/qt/qtwayland) / [grefsen](https://github.com/ec1oud/grefsen) and [Mir](https://mir-server.io) / [egmde](https://github.com/AlanGriffiths/egmde). These are pretty cool and still worth exploring further.

Lubuntu have [declared](https://lubuntu.me/lubuntu-development-newsletter-9/) that they will be switching to Wayland by default for 20.10 and that they are going to do this by porting Openbox to use the Mir display server and [Drew DeVault’s](https://drewdevault.com/) QtLayerShell, etc. One to keep an eye on.

<b>kwin and mutter</b>

I don't think that the KDE and GNOME compositors will be right. Although they offer a brilliant experience, they are pretty heavy and quite integrated with their respective stacks. I think I'm right in saying that they're not standalone window managers.

[mutter](https://gitlab.gnome.org/GNOME/mutter) - 411k lines-of-code (LOC)

[kwin](https://github.com/KDE/kwin) - 191k LOC

In terms of size comparison of these two giants, it's worth reflecting on the size of a few friends:

[sway](https://github.com/swaywm/sway) - 37k LOC

[rootston](https://github.com/swaywm/wlroots/tree/master/rootston) - 7k LOC

[openbox](https://github.com/danakj/openbox) - 53k LOC

[i3](https://github.com/i3/i3) - 20k LOC (but does include i3bar, etc)

[dwm](https://dwm.suckless.org) - 2k LOC

