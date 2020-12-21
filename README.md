# labwc

- [1. What is this?](#1-what-is-this)
- [2. Build](#2-build)
- [3. Configure](#3-configure)
- [4. Run](#4-run)
- [5. Integrate](#5-integrate)
- [6. Roadmap](#6-roadmap)
- [7. Scope](#7-scope)

## 1. What is this?

Labwc is a [WIP] free, stacking compositor for Wayland based on wlroots.

It has the following aims:

- Be light-weight, small and fast
- Have the look and feel of [openbox](https://github.com/danakj/openbox) albeit
  with smaller feature set
- Where practicable, use clients to show wall-paper, take screenshots, and so on
- Stay in keeping with wlroots and sway in terms of approach and coding style

It is in early development, so expect bugs and missing features.

Labwc has been inspired and influenced by [openbox](https://github.com/danakj/openbox), [sway](https://github.com/swaywm/sway), [cage](https://www.hjdskes.nl/blog/cage-01/), [wio](https://wio-project.org/) and [rootston](https://github.com/swaywm/rootston)

The following were considered before choosing wlroots: [qtwayland](https://github.com/qt/qtwayland), [grefsen](https://github.com/ec1oud/grefsen), [mir](https://mir-server.io) and [egmde](https://github.com/AlanGriffiths/egmde).

![](https://raw.githubusercontent.com/wiki/johanmalm/labwc/images/scrot2.png)

## 2. Build

    meson build && ninja -C build

Dependencies include:

- meson, ninja
- wlroots (>0.12)
- wayland (>=1.16)
- wayland-protocols
- xwayland
- libinput (>=1.14)
- libxml2
- cairo, pango, glib-2.0
- xcb
- xkbcommon

For further details see [wiki/Build](https://github.com/johanmalm/labwc/wiki/Build).

It seems a bit early to start tagging, but if you're running wlroots 0.10-0.12, checkout commit 071fcc6

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
- Alt-F2: cycle window
- Alt-F3: launch dmenu
- Alt-escape: exit

## 5. Integrate

Suggested apps to use with labwc:

- [grim](https://github.com/emersion/grim) - Screenshoter
- [wf-recorder](https://github.com/ammen99/wf-recorder) - Screen-recorder
- [swaybg](https://github.com/swaywm/swaybg) - Background image
- [waybar](https://github.com/Alexays/Waybar) - Panel
- [bemenu](https://github.com/Cloudef/bemenu) - Launcher
- [fuzzel](https://codeberg.org/dnkl/fuzzel) - Launcher
- [wofi](https://hg.sr.ht/~scoopta/wofi) - Launcher

## 6. Roadmap

No acceptance criteria exists, but the following list indicates the inteded high level roadmap:

- [x] Support xwayland
- [x] Parse openbox config files (rc.xml, autostart, environment)
- [x] Parse openbox themes and associated xbm icons
- [x] Show maximize, iconify, close buttons
- [x] Catch SIGHUP to re-load config file and theme
- [x] Support layer-shell protocol ('exclusive' not yet implemented)
- [ ] Support root-menu and parse menu.xml (very simple implementation, not submenus yet)
- [ ] Support 'maximize'
- [ ] Support wlr-output-management protocol and [kanshi](https://github.com/emersion/kanshi.git)
- [ ] Show window title
- [ ] Support foreign-toplevel protocol (e.g. to integrate with wlroots panels/bars)
- [ ] Support damage tracking to reduce CPU usage
- [ ] Implement client-menu
- [ ] Support on-screen display (osd), for example to support alt-tab window list
- [ ] Support HiDPI

## 7. Scope

In order to keep the code base clean and maintainable, simplicy is favoured over full specification adherence.

### In-scope

Refer to these wiki pages for scope info:

- [progress](https://github.com/johanmalm/labwc/wiki/Scope-progress)
- [configuration](https://github.com/johanmalm/labwc/wiki/Scope-configuration)
- [specification](https://github.com/johanmalm/labwc/wiki/Scope-theme-specification)
- [actions](https://github.com/johanmalm/labwc/wiki/Scope-actions)

### Out-of-scope

The following items are out-of-scope:

- Icons (except window buttons)
- Animations
- Gradients on window decorations and menus
- Any theme option (probably at least half of them) not required to reasonably render common themes
- Any configuration option not required to provide a simple openbox-like experience
- Multiple desktops

