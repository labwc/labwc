# labwc

- [1. What is this?](#1-what-is-this)
- [2. Build](#2-build)
- [3. Configure](#3-configure)
- [4. Run](#4-run)
- [5. Integrate](#5-integrate)
- [6. Roadmap](#6-roadmap)

## 1. What is this?

Labwc is a [WIP] free, stacking compositor for Wayland and has the following aims:

- Be light-weight, small and fast
- Have the look and feel of [openbox](https://github.com/danakj/openbox)
- Where practicable, use other software to show wall-paper, take screenshots,
  and so on

It is in early development, so expect bugs and missing features.

Labwc has been inspired and influenced by [openbox](https://github.com/danakj/openbox), [sway](https://github.com/swaywm/sway), [cage](https://www.hjdskes.nl/blog/cage-01/), [wio](https://wio-project.org/) and [rootston](https://github.com/swaywm/rootston)

Labwc is based on the wlroots library. The following were considered before choosing wlroots: [qtwayland](https://github.com/qt/qtwayland), [grefsen](https://github.com/ec1oud/grefsen), [mir](https://mir-server.io) and [egmde](https://github.com/AlanGriffiths/egmde).

![](https://raw.githubusercontent.com/wiki/johanmalm/labwc/images/scrot1.png)

## 2. Build

    meson build && ninja -C build

Runtime dependencies include:

- wlroots (>=0.10.0)
- xwayland
- libxml2
- cairo
- pango

For further details see [tools/build](tools/build) and [wiki/Build](https://github.com/johanmalm/labwc/wiki/Build).

## 3. Configure

See [rc.xml](data/rc.xml) and [themerc](data/themes/labwc-default/openbox-3/themerc) comments for details including keybinds.

If you want to override the defaults, copy data/rc.xml to ~/.config/labwc/ and tweak to suit.

Full details in [labwc(1)](docs/labwc.1.md) and [labwc(5)](docs/labwc.5.md)

## 4. Run

    ./build/labwc -s <some-application>

## 5. Integrate

Suggested apps to use with labwc:

- [grim](https://github.com/emersion/grim) - Take screenshot

## 6. Roadmap

- [x] Support xwayland
- [x] Parse [rc.xml](data/rc.xml)
- [x] Parse [themerc](data/themes/labwc-default/openbox-3/themerc)
- [x] Read xbm icons
- [x] Show maximize, minimize, close buttons
- [ ] Give actions to maximize, minimize, close buttons
- [ ] Add grip
- [ ] Support layer-shell background (e.g. using swaybg)
- [ ] Draw better alt-tab rectangle
- [ ] Try restarting and consider catching SIGHUP for --reconfigure
- [ ] Implement client-menu
- [ ] Implement root-menu

For further details see [wiki/Roadmap](https://github.com/johanmalm/labwc/wiki/Roadmap).

