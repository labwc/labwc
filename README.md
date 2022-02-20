# labwc

- [1. What is this?](#1-what-is-this)
- [2. Build](#2-build)
- [3. Install](#3-install)
- [4. Configure](#4-configure)
- [5. Run](#5-run)
- [6. Integrate](#6-integrate)
- [7. Roadmap](#7-roadmap)

## 1. What is this?

`labwc` stands for Lab Wayland Compositor, where lab can mean any of the following:

- sense of experimentation and treading new ground
- inspired by BunsenLabs and ArchLabs
- your favorite pet

It is a wlroots-based stacking compositor aiming to be lightweight and independent, with a focus on simply stacking windows well and rendering some window decorations. It relies on clients for wallpaper, panels, screenshots, and so on to create a full desktop environment.

Labwc tries to stay in keeping with wlroots and sway in terms of general approach and coding style.

In order to avoid reinventing configuration and theme syntax, the [openbox-3.4] specification is used. This does not mean that labwc is an openbox clone but rather that configuration files will look and feel familiar.

| video link     | date        | content
| -------------- | ------------| -------
| [Video (0:18)] | 16-Oct-2021 | SnapToEdge feature
| [Video (1:10)] | 05-Aug-2021 | window gymnastics, theming and waybar
| [Video (3:42)] | 25-Feb-2021 | setting background and themes; xwayland/xdg-shell windows

<a href="https://raw.githubusercontent.com/wiki/labwc/labwc/images/scrot3.png">
  <img src="https://raw.githubusercontent.com/wiki/labwc/labwc/images/scrot3x.png" width="256px" height="179px">
</a>

So far, labwc supports the following:

- [x] Config files (rc.xml, autostart, environment)
- [x] Theme files and xbm icons
- [x] Damage tracking to reduce CPU usage
- [x] A basic root-menu (configured with menu.xml)
- [x] HiDPI
- [x] wlr-output-management protocol
- [x] layer-shell protocol
- [x] foreign-toplevel protocol (e.g. to integrate with panels and bars)
- [x] Optionally xwayland

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

For further details see [wiki/Build](https://github.com/labwc/labwc/wiki/Build).

## 3. Install

See [wiki/Install](https://github.com/labwc/labwc/wiki/Install).

## 4. Configure

Labwc uses the files listed below for configuration and theming.

| file          | user over-ride location                         | man page
| ------------- | ----------------------------------------------- | --------
| [rc.xml]      | ~/.config/labwc/                                | [labwc-config(5)], [labwc-actions(5)]
| [menu.xml]    | ~/.config/labwc/                                | [labwc-menu(5)]
| [autostart]   | ~/.config/labwc/                                | [labwc(1)]
| [environment] | ~/.config/labwc/                                | [labwc-environment(5)]
| [themerc]     | ~/.local/share/themes/\<theme-name\>/openbox-3/ | [labwc-theme(5)]

The example [rc.xml] has been kept simple. For all options and default values, see [rc.xml.all]

Configuration and theme files are reloaded on receiving SIGHUP (e.g. `killall -SIGHUP labwc`)

For keyboard settings, see [environment] and [xkeyboard-config(7)]

For themes, search the internet for "openbox themes" and place them in `~/.local/share/themes/`. Some good starting points include:

- https://github.com/addy-dclxvi/openbox-theme-collections
- https://github.com/the-zero885/Lubuntu-Arc-Round-Openbox-Theme
- https://bitbucket.org/archlabslinux/themes/
- https://github.com/BunsenLabs/bunsen-themes

## 5. Run

    ./build/labwc [-s <command>]

Click on the background to launch a menu.

If you have not created an rc.xml config file, default bindings will be:

| combination              | action
| ------------------------ | ------
| `alt`-`tab`              | activate next window
| `alt`-`escape`           | exit
| `super`-`return`         | alacritty
| `alt`-`F3`               | bemenu
| `alt`-`F4`               | close window
| `super`-`a`              | toggle maximize
| `alt`-`mouse-left`       | move window
| `alt`-`mouse-right`      | resize window
| `alt`-`arrow`            | move window to edge
| `super`-`arrow`          | resize window to fill half the output
| `XF86_AudioLowerVolume`  | amixer sset Master 5%-
| `XF86_AudioRaiseVolume`  | amixer sset Master 5%+
| `XF86_AudioMute`         | amixer sset Master toggle
| `XF86_MonBrightnessUp`   | brightnessctl set +10%
| `XF86_MonBrightnessDown` | brightnessctl set 10%-

## 6. Integrate

Suggested apps to use with labwc:

- Screen shooter: [grim]
- Screen recorder: [wf-recorder]
- Background image: [swaybg]
- Panel: [waybar], [yambar], [lavalauncher]
- Launchers: [bemenu], [fuzzel], [wofi]
- Output managers: [kanshi], [wlr-randr]
- Screen locker: [swaylock]

## 7. Roadmap

A lot of emphasis is put on code simplicity when considering features.

The main development effort is focused on producing a solid foundation for a
stacking compositor rather than adding configuration and theming options.

See [acceptance criteria] for details.

High-level summary of items which are not intended to be implemented:

- Icons (except window buttons)
- Animations
- Gradients for decoration and menus
- Any theme option not required to reasonably render common themes (it's amazing
  how few options are actually required).


[openbox-3.4]: http://openbox.org/wiki/Help:Contents
[rc.xml]: docs/rc.xml
[rc.xml.all]: docs/rc.xml.all
[menu.xml]: docs/menu.xml
[autostart]: docs/autostart
[environment]: docs/environment
[themerc]: docs/themerc

[labwc(1)]: https://raw.githubusercontent.com/labwc/labwc/master/docs/labwc.1.scd
[labwc-config(5)]: https://raw.githubusercontent.com/labwc/labwc/master/docs/labwc-config.5.scd
[labwc-menu(5)]: https://raw.githubusercontent.com/labwc/labwc/master/docs/labwc-menu.5.scd
[labwc-environment(5)]: https://raw.githubusercontent.com/labwc/labwc/master/docs/labwc-environment.5.scd
[labwc-theme(5)]: https://raw.githubusercontent.com/labwc/labwc/master/docs/labwc-theme.5.scd
[labwc-actions(5)]: https://raw.githubusercontent.com/labwc/labwc/master/docs/labwc-actions.5.scd
[xkeyboard-config(7)]: https://manpages.debian.org/testing/xkb-data/xkeyboard-config.7.en.html

[wiki/Build]: https://github.com/labwc/labwc/wiki/Build

[grim]: https://github.com/emersion/grim
[wf-recorder]: https://github.com/ammen99/wf-recorder
[swaybg]: https://github.com/swaywm/swaybg
[waybar]: https://github.com/Alexays/Waybar
[yambar]: https://codeberg.org/dnkl/yambar
[lavalauncher]: https://sr.ht/~leon_plickat/LavaLauncher
[bemenu]: https://github.com/Cloudef/bemenu
[fuzzel]: https://codeberg.org/dnkl/fuzzel
[wofi]: https://hg.sr.ht/~scoopta/wofi
[kanshi]: https://sr.ht/~emersion/kanshi/
[wlr-randr]: https://sr.ht/~emersion/wlr-randr/
[swaylock]: https://github.com/swaywm/swaylock

[acceptance criteria]: https://github.com/labwc/labwc-acceptance-criteria/blob/main/Acceptance-criteria.md

[Video (0:18)]: https://user-images.githubusercontent.com/21316711/137605766-67196fb0-c147-4114-a8f7-1651580c2572.mp4
[Video (1:10)]: https://youtu.be/AU_M3n_FS-E
[Video (3:42)]: https://youtu.be/rE1bQjSVJzg

