# labwc

- [1. What is this?](#1-what-is-this)
- [2. Build](#2-build)
- [3. Install](#3-install)
- [4. Configure](#4-configure)
- [5. Run](#5-run)
- [6. Integrate](#6-integrate)
- [7. Roadmap](#7-roadmap)
- [8. Scope](#8-scope)

## 1. What is this?

Labwc is a wlroots-based stacking compositor for Wayland.

It has the following aims:

- Be light-weight, small and fast
- Keep feature set fairly small and use [openbox-3.4](https://github.com/danakj/openbox)
  configuration/theme specification in order to avoid inventing another one.
- Where practicable, use clients to show wall-paper, take screenshots, and so on
- Stay in keeping with wlroots and sway in terms of approach and coding style

[Video (3:42) showing features](https://youtu.be/rE1bQjSVJzg)

<a href="https://raw.githubusercontent.com/wiki/johanmalm/labwc/images/scrot2.png"><img src="https://raw.githubusercontent.com/wiki/johanmalm/labwc/images/scrot2x.png" width="256px" height="144px"></a>

<a href="https://raw.githubusercontent.com/wiki/johanmalm/labwc/images/scrot3.png"><img src="https://raw.githubusercontent.com/wiki/johanmalm/labwc/images/scrot3x.png" width="256px" height="179px"></a>

## 2. Build

    meson build/
    ninja -C build/

Dependencies include:

- meson, ninja, gcc/clang
- wlroots (master)
- wayland (>=1.19)
- wayland-protocols
- libinput (>=1.14)
- libxml2
- cairo, pango, glib-2.0
- xkbcommon
- xwayland, xcb (optional)

Disable xwayland with `meson -Dxwayland=disabled build/`

For further details see [wiki/Build](https://github.com/johanmalm/labwc/wiki/Build).

## 3. Install

See [wiki/Install](https://github.com/johanmalm/labwc/wiki/Install).

## 4. Configure

If you want to override the defaults, create the following files:

- ~/.config/labwc/[rc.xml](docs/rc.xml)
- ~/.config/labwc/[menu.xml](docs/menu.xml)
- ~/.local/share/themes/\<theme-name\>/openbox-3/[themerc](docs/themerc)

See full details in the following:

- [labwc(1)](docs/labwc.1.scd)
- [labwc-config(5)](docs/labwc-config.5.scd)
- [labwc-theme(5)](docs/labwc-theme.5.scd)
- [labwc-actions(5)](docs/labwc-actions.5.scd)

## 5. Run

    ./build/labwc [-s <command>]

Click on the background to launch a menu.

If you have not created an rc.xml configuration file, default keybinds will be:

- Alt-tab: cycle window
- Alt-F3: launch bemenu
- Alt-escape: exit

## 6. Integrate

Suggested apps to use with labwc:

- Screen-shooter: [grim](https://github.com/emersion/grim)
- Screen-recorder: [wf-recorder](https://github.com/ammen99/wf-recorder)
- Background image: [swaybg](https://github.com/swaywm/swaybg)
- Panel: [waybar](https://github.com/Alexays/Waybar)
- Launchers: [bemenu](https://github.com/Cloudef/bemenu), [fuzzel](https://codeberg.org/dnkl/fuzzel), [wofi](https://hg.sr.ht/~scoopta/wofi)
- Output managers: [kanshi](https://github.com/emersion/kanshi.git), [wlr-randr](https://github.com/emersion/wlr-randr.git)

## 7. Acceptance Criteria

The following list indicates the intended high level roadmap:

- [x] Optionally support xwayland
- [x] Parse openbox config files (rc.xml, autostart, environment)
- [x] Parse openbox themes files and associated xbm icons
- [x] Support maximize, iconify, close buttons
- [x] Catch SIGHUP to re-load config file and theme
- [x] Support layer-shell protocol
- [x] Support damage tracking to reduce CPU usage
- [x] Parse menu.xml to generate a basic root-menu
- [x] Support wlr-output-management protocol
- [ ] Support HiDPI
- [ ] Support foreign-toplevel protocol (e.g. to integrate with wlroots panels/bars)
- [ ] Support on-screen display (osd), for example to support alt-tab window list
- [ ] Support libinput configuration (tap is enabled for the time being)

A lot of emphasis is put on code simplicy when considering features. The main development
effort if focused on producing a solid foundation for a stacking compositor
rather than quickly adding configuration and theming options.

The following items are likely to be out-of-scope:

- Icons (except window buttons)
- Animations
- Gradients on window decorations and menus
- Any theme option not required to reasonably render common themes (amazingig
  how few options are actually required).
- Any configuration option not required to provide a simple openbox-like experience

See [wiki/Acceptance-criteria](https://github.com/johanmalm/labwc/wiki/Acceptance-criteria)
for a view of the full down-selection.

