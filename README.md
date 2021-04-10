# labwc

- [1. What is this?](#1-what-is-this)
- [2. Build](#2-build)
- [3. Install](#3-install)
- [4. Configure](#4-configure)
- [5. Run](#5-run)
- [6. Integrate](#6-integrate)
- [7. Accept](#7-acceptance-criteria)

## 1. What is this?

Labwc is a wlroots-based stacking compositor for Wayland.

It aims to be light-weight and independent, with a focus on simply
stacking windows well and rendering some window decorations. Where
practicable, it uses clients for wall-paper, panels, screenshots, and so
on.

Labwc tries to stay in keeping with wlroots and sway in terms of general
approach and coding style.

In order to avoid re-inventing configuration syntax and theme variable
names, [openbox-3.4] specification is used. This does not mean that
labwc is an openbox clone. In fact, as a Wayland compositor it will be
quite different in areas, and the acceptance criteria only lists ca
40% of openbox features.

Parsing GTK3 and Qt themes for window decorations is quite complicated,
so using the much simpler openbox specification makes sense for a
simple compositor such as labwc.

[Video (3:42)](https://youtu.be/rE1bQjSVJzg)

<a href="https://raw.githubusercontent.com/wiki/johanmalm/labwc/images/scrot2.png">
  <img src="https://raw.githubusercontent.com/wiki/johanmalm/labwc/images/scrot2x.png" width="256px" height="144px">
</a>  
<a href="https://raw.githubusercontent.com/wiki/johanmalm/labwc/images/scrot3.png">
  <img src="https://raw.githubusercontent.com/wiki/johanmalm/labwc/images/scrot3x.png" width="256px" height="179px">
</a>

## 2. Build

    meson build/
    ninja -C build/

Dependencies include:

- meson, ninja, gcc/clang
- wlroots (>=0.13.0 or master)
- wayland (>=1.19)
- wayland-protocols
- libinput (>=1.14)
- libxml2
- cairo, pango, glib-2.0
- xkbcommon
- xwayland, xcb (optional)

Disable xwayland with `meson -Dxwayland=disabled build/`

For further details see [wiki/Build].

## 3. Install

See [wiki/Install](https://github.com/johanmalm/labwc/wiki/Install).

## 4. Configure

Labwc uses the files listed below for configuration and theming.

| file          | user over-ride location                         | man page
| ------------- | ----------------------------------------------- | --------
| [rc.xml]      | ~/.config/labwc/                                | [labwc-config(5)]
| [menu.xml]    | ~/.config/labwc/                                | [labwc-menu(5)]
| [autostart]   | ~/.config/labwc/                                | [labwc(1)]
| [environment] | ~/.config/labwc/                                | [labwc-environment(5)]
| [themerc]     | ~/.local/share/themes/\<theme-name\>/openbox-3/ | [labwc-theme(5)]

See also [labwc(1)] and [labwc-actions(5)]

## 5. Run

    ./build/labwc [-s <command>]

Click on the background to launch a menu.

If you have not created an rc.xml configuration file, default keybinds will be:

- Alt-tab: cycle window
- Alt-F3: launch bemenu
- Alt-escape: exit

## 6. Integrate

Suggested apps to use with labwc:

- Screen-shooter: [grim]
- Screen-recorder: [wf-recorder]
- Background image: [swaybg]
- Panel: [waybar]
- Launchers: [bemenu], [fuzzel], [wofi]
- Output managers: [kanshi], [wlr-randr]

## 7. Acceptance Criteria

A lot of emphasis is put on code simplicy when considering features.

The main development effort if focused on producing a solid foundation for a
stacking compositor rather than adding configuration and theming options.

In order to define what 'small feature set' means, refer to the lists of
[complete] and [outstanding] items.

For more details, see the full table of [acceptance criteria].

High-level summary of progress:

- [x] Optionally support xwayland
- [x] Parse openbox config files (rc.xml, autostart, environment)
- [x] Parse openbox themes files and associated xbm icons
- [x] Support maximize, iconify, close buttons
- [x] Catch SIGHUP to re-load config file and theme
- [x] Support layer-shell protocol
- [x] Support damage tracking to reduce CPU usage
- [x] Parse menu.xml to generate a basic root-menu
- [x] Support wlr-output-management protocol
- [x] Support HiDPI
- [ ] Support foreign-toplevel protocol (e.g. to integrate with wlroots panels/bars)
- [ ] Support on-screen display (osd), for example to support alt-tab window list
- [ ] Support libinput configuration (tap is enabled for the time being)

High-level summary of items which are not inteded to be implemented:

- Icons (except window buttons)
- Animations
- Gradients for decoration and menus
- Any theme option not required to reasonably render common themes (it's amazing
  how few options are actually required).

[openbox-3.4]: https://github.com/danakj/openbox

[rc.xml]: docs/rc.xml
[menu.xml]: docs/menu.xml
[autostart]: docs/autostart
[environment]: docs/environment
[themerc]: docs/themerc

[labwc(1)]: https://raw.githubusercontent.com/johanmalm/labwc/master/docs/labwc.1.scd
[labwc-config(5)]: https://raw.githubusercontent.com/johanmalm/labwc/master/docs/labwc-config.5.scd
[labwc-menu(5)]: https://raw.githubusercontent.com/johanmalm/labwc/master/docs/labwc-menu.5.scd
[labwc-environment(5)]: https://raw.githubusercontent.com/johanmalm/labwc/master/docs/labwc-environment.5.scd
[labwc-theme(5)]: https://raw.githubusercontent.com/johanmalm/labwc/master/docs/labwc-theme.5.scd
[labwc-actions(5)]: https://raw.githubusercontent.com/johanmalm/labwc/master/docs/labwc-actions.5.sc

[wiki/Build]: https://github.com/johanmalm/labwc/wiki/Build

[grim]: https://github.com/emersion/grim
[wf-recorder]: https://github.com/ammen99/wf-recorder
[swaybg]: https://github.com/swaywm/swaybg
[waybar]: https://github.com/Alexays/Waybar
[bemenu]: https://github.com/Cloudef/bemenu
[fuzzel]: https://codeberg.org/dnkl/fuzzel
[wofi]: https://hg.sr.ht/~scoopta/wofi
[kanshi]: https://github.com/emersion/kanshi.git
[wlr-randr]: https://github.com/emersion/wlr-randr.git

[acceptance criteria]: https://github.com/johanmalm/labwc/wiki/Acceptance-criteria
[complete]: https://github.com/johanmalm/labwc/wiki/Minimum-viable-product-complete-items
[outstanding]: https://github.com/johanmalm/labwc/wiki/Minimum-viable-product-outstanding-items

