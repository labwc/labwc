# labwc

Labwc is a [WIP] free, stacking compositor for Wayland and has the following aims:

- Be light-weight, small and fast
- Have the look and feel of Openbox
- Where practicable, use other software to show wall-paper, take screenshots,
  and so on

It is in early development, so expect bugs and missing features.

![](https://raw.githubusercontent.com/wiki/johanmalm/labwc/images/scrot1.png)

## Table of Contents

- [Dependencies](#dependencies)
- [Roadmap](#roadmap)
- [Inspiration](#inspiration)
- [Design](#design)
- [Configuration](#configuration)
- [Integration](#integration)
- [Build](#build)

## Dependencies

Runtime dependencies include:

- wlroots (>=0.10.0)
- xwayland
- libxml2
- cairo
- pango

## Roadmap

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

## Inspiration

Labwc has been inspired and inflenced by [openbox](https://github.com/danakj/openbox), [sway](https://github.com/swaywm/sway), [cage](https://www.hjdskes.nl/blog/cage-01/), [wio](https://wio-project.org/) and [rootston](https://github.com/swaywm/rootston)

## Design

Labwc is based on the wlroots library.

The following were considered before choosing wlroots: [qtwayland](https://github.com/qt/qtwayland), [grefsen](https://github.com/ec1oud/grefsen), [mir](https://mir-server.io) and [egmde](https://github.com/AlanGriffiths/egmde).

## Configuration

See [rc.xml](data/rc.xml) and [themerc](data/themes/labwc-default/openbox-3/themerc) comments for details including keybinds.

Full details in [labwc(1)](docs/labwc.1.md) and [labwc(5)](docs/labwc.5.md)

Suggest either copying data/rc.xml to ~/.config/labwc/running, or running with:

    ./build/labwc -c data/rc.xml

## Integration

Suggested apps:

- [grim](https://github.com/emersion/grim) - screenshots

## Build

    meson build && ninja -C build

For further details see [tools/build](tools/build) and [wiki/Build](https://github.com/johanmalm/labwc/wiki/Build).

