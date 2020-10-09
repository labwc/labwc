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

To build you also need headers for:

- xcb
- xkbcommon

For further details see [wiki/Build](https://github.com/johanmalm/labwc/wiki/Build).

## 3. Configure

If you want to override the defaults, copy data/rc.xml to ~/.config/labwc/ and tweak to suit.

See [rc.xml](docs/rc.xml) and [themerc](docs/themerc) comments for details including keybinds.

See full details in the following:

- [labwc(1)](docs/labwc.1.md)
- [labwc-config(5)](docs/labwc-config.5.md)
- [labwc-theme(5)](docs/labwc-theme.5.md)
- [labwc-actions(5)](docs/labwc-actions.5.md)

## 4. Run

    ./build/labwc -s <some-application>

If you have not created an rc.xml configuration file, default keybinds will be:

- Alt-tab: cycle window
- Alt-F2: cycle window (useful if running under X11 and alt-tab is already bound to something)
- Alt-F3: launch dmenu (if installed)
- Alt-escape: exit

## 5. Integrate

Suggested apps to use with labwc:

- [grim](https://github.com/emersion/grim) - Take screenshot
- [wf-recorder](https://github.com/ammen99/wf-recorder) - Record screen
- [swaybg](https://github.com/swaywm/swaybg) - Set background image
- [waybar](https://github.com/Alexays/Waybar) - Panel

## 6. Roadmap

- [x] Support xwayland
- [x] Parse ~/.config/labwc/{rc.xml,autostart,environment}
- [x] Parse /usr/share/themes/`<name>`/openbox-3/themerc and associated xbm icons
- [x] Show maximize, iconify, close buttons
- [x] Catch SIGHUP to re-load config file and theme
- [x] Support layer-shell protocol (partial)
- [ ] Support 'maximize'
- [ ] Show window title
- [ ] Support foreign-toplevel protocol (e.g. to integrate with wlroots panels/bars)
- [ ] Support damage control to reduce CPU usage
- [ ] Implement client-menu
- [ ] Implement root-menu
- [ ] Support on-screen display (OSD), for example to support alt-tab window list
- [ ] Support [kanshi](https://github.com/emersion/kanshi.git)

For further details see [wiki/Roadmap](https://github.com/johanmalm/labwc/wiki/Roadmap).

Based on development so far, it looks like only a modest fraction of all theme and configuration options are required to adequately render most common themes and provide a pretty openbox-like experience.

It is likely that only a subset of the full specification will be implemented in order to keep the code base simpler and cleaner.

