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
- Have the look and feel of [openbox](https://github.com/danakj/openbox) albeit
  with a smaller feature set
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
- wlroots (0.11.0 - 0.12.0)
- wayland (>=1.16)
- wayland-protocols
- libinput (>=1.14)
- libxml2
- cairo, pango, glib-2.0
- xkbcommon
- xwayland, xcb (optional)

Disable xwayland with `meson -Dxwayland=disabled build`

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

- [grim](https://github.com/emersion/grim) - Screenshoter
- [wf-recorder](https://github.com/ammen99/wf-recorder) - Screen-recorder
- [swaybg](https://github.com/swaywm/swaybg) - Background image
- [waybar](https://github.com/Alexays/Waybar) - Panel
- [bemenu](https://github.com/Cloudef/bemenu) - Launcher
- [fuzzel](https://codeberg.org/dnkl/fuzzel) - Launcher
- [wofi](https://hg.sr.ht/~scoopta/wofi) - Launcher

## 7. Roadmap

The following list indicates the intended high level roadmap:

- [x] Optionally support xwayland
- [x] Parse openbox config files (rc.xml, autostart, environment)
- [x] Parse openbox themes files and associated xbm icons
- [x] Support maximize, iconify, close buttons
- [x] Catch SIGHUP to re-load config file and theme
- [ ] Support layer-shell protocol
	- [x] Support layer-shell applications
	- [ ] Support 'exclusive' zone
- [x] Support damage tracking to reduce CPU usage
- [ ] Support menus
	- [x] Parse menu.xml and generate root-menu
	- [ ] Separators and titles
	- [ ] Submenus
	- [ ] Pipe-menus
	- [ ] Client menus
- [ ] Support wlr-output-management protocol and [kanshi](https://github.com/emersion/kanshi.git)
- [ ] Support foreign-toplevel protocol (e.g. to integrate with wlroots panels/bars)
- [ ] Support on-screen display (osd), for example to support alt-tab window list
- [ ] Support HiDPI
- [ ] Support libinput configuration (tap is enabled for the time being)

## 8. Scope

In order to keep the code base clean and maintainable, simplicy is favoured over full specification adherence.

The exact acceptance criteria have not yet been defined, but the following wiki pages give an indication of the intended scope definition against the openbox specification.

- [configuration](https://github.com/johanmalm/labwc/wiki/Scope-configuration)
- [theme](https://github.com/johanmalm/labwc/wiki/Scope-theme-specification)
- [actions](https://github.com/johanmalm/labwc/wiki/Scope-actions)

The following items are likely to be out-of-scope:

- Icons (except window buttons)
- Animations
- Gradients on window decorations and menus
- Any theme option (probably at least half of them) not required to reasonably render common themes
- Any configuration option not required to provide a simple openbox-like experience

