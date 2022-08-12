# labwc

<h3 align="center">[<a
href="https://labwc.github.io/">Website</a>] [<a
href="https://github.com/labwc/labwc-scope#readme">Scope</a>] [<a
href="https://web.libera.chat/gamja/?channels=#labwc">IRC&nbsp;Channel</a>] [<a
href="NEWS.md">Release&nbsp;Notes</a>]</h3>

- [1. What is this?](#1-what-is-this)
- [2. Build and Installation](#2-build-and-installation)
- [3. Configuration](#3-configuration)
- [4. Theming](#4-theming)
- [5. Usage](#5-usage)
- [6. Integration](#6-integration)
- [7. Scope](#7-scope)

## 1. What is this?

Labwc stands for Lab Wayland Compositor, where lab can mean any of the
following:

- sense of experimentation and treading new ground
- inspired by BunsenLabs and ArchLabs
- your favorite pet

Labwc is a [wlroots]-based window-stacking compositor for [wayland], inspired by
[openbox].

It is light-weight and independent with a focus on simply stacking windows well
and rendering some window decorations. It takes a no-bling/frills approach and
says no to features such as icons (except window buttons), animations,
decorative gradients and any other options not required to reasonably render
common themes. It relies on clients for panels, screenshots, wallpapers and so
on to create a full desktop environment.

Labwc tries to stay in keeping with [wlroots] and [sway] in terms of general
approach and coding style.

Labwc only understands [wayland-protocols] &amp; [wlr-protocols], and it cannot
be controlled with dbus, sway/i3-IPC or other technology. The reason for this is
that we believe that custom IPCs and protocols create a fragmentation that
hinders general Wayland adoption.

In order to avoid reinventing configuration and theme syntax, the [openbox] 3.6
specification is used. This does not mean that labwc is an openbox clone but
rather that configuration files will look and feel familiar.

Labwc supports the following:

- [x] Config files (rc.xml, autostart, environment, menu.xml)
- [x] Theme files and xbm icons
- [x] Basic root-menu and client-menu
- [x] HiDPI
- [x] wlroots protocols such as `output-management`, `layer-shell` and
  `foreign-toplevel`
- [x] Optionally xwayland

See [scope] for full details on implemented features.

<a href="https://i.imgur.com/vOelinT.png">
  <img src="https://i.imgur.com/vOelinTl.png">
</a>

| video link     | date        | content
| -------------- | ------------| -------
| [Video (0:18)] | 16-Oct-2021 | SnapToEdge feature
| [Video (1:10)] | 05-Aug-2021 | window gymnastics, theming and waybar
| [Video (3:42)] | 25-Feb-2021 | setting background and themes; xwayland/xdg-shell windows

## 2. Build and Installation

To build, simply run:

    meson build/
    ninja -C build/

Run-time dependencies include:

- wlroots, wayland, libinput, xkbcommon
- libxml2, cairo, pango, glib-2.0
- xwayland, xcb (optional)

Build dependencies include:

- meson, ninja, gcc/clang
- wayland-protocols

Disable xwayland with `meson -Dxwayland=disabled build/`

For OS/distribution specific details see see [wiki].

## 3. Configuration

For a step-by-step initial configuration guide, see [getting-started]

User config files are located at `${XDG_CONFIG_HOME:-$HOME/.config/labwc/}`
with the following four files being used:

| file          | man page
| ------------- | --------
| [rc.xml]      | [labwc-config(5)], [labwc-actions(5)]
| [menu.xml]    | [labwc-menu(5)]
| [autostart]   | [labwc(1)]
| [environment] | [labwc-config(5)]

The example [rc.xml] has been kept simple. For all options and default values,
see [rc.xml.all]

Configuration and theme files are reloaded on receiving SIGHUP
(e.g. `killall -s SIGHUP labwc`)

For keyboard settings, see [environment] and [xkeyboard-config(7)]

## 4. Theming

Themes are located at `~/.local/share/themes/\<theme-name\>/openbox-3/` or
equivalent `XDG_DATA_{DIRS,HOME}` location in accordance with freedesktop XDG
directory specification.

For full theme options, see [labwc-theme(5)] or the [themerc] example file.

For themes, search the internet for "openbox themes" and place them in
`~/.local/share/themes/`. Some good starting points include:

- https://github.com/addy-dclxvi/openbox-theme-collections
- https://github.com/the-zero885/Lubuntu-Arc-Round-Openbox-Theme
- https://bitbucket.org/archlabslinux/themes/
- https://github.com/BunsenLabs/bunsen-themes

## 5. Usage

    ./build/labwc [-s <command>]

> **_NOTE:_** If you are running on **NVIDIA**, you will need the
> `nvidia-drm.modeset=1` kernel parameter.

If you have not created an rc.xml config file, default bindings will be:

| combination              | action
| ------------------------ | ------
| `alt`-`tab`              | activate next window
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

A root-menu can be opened by clicking on the desktop.

## 6. Integration

Suggested apps to use with labwc:

- Screen shooter: [grim]
- Screen recorder: [wf-recorder]
- Background image: [swaybg]
- Panel: [waybar], [yambar], [lavalauncher], [sfwbar]
- Launchers: [bemenu], [fuzzel], [wofi]
- Output managers: [wlopm], [kanshi], [wlr-randr]
- Screen locker: [swaylock]

See [integration] for further details.

## 7. Scope

A lot of emphasis is put on code simplicity when considering features.

The main development effort is focused on producing a solid foundation for a
stacking compositor rather than adding configuration and theming options.

See [scope] for details.

High-level summary of items which are not intended to be implemented:

- Icons (except window buttons)
- Animations
- Gradients for decoration and menus
- Any theme option not required to reasonably render common themes (it is
  amazing how few options are actually required).

[wayland]: https://wayland.freedesktop.org/
[openbox]: http://openbox.org/wiki/Help:Contents
[wlroots]: https://gitlab.freedesktop.org/wlroots/wlroots
[sway]: https://github.com/swaywm
[wayland-protocols]: https://gitlab.freedesktop.org/wayland/wayland-protocols
[wlr-protocols]: https://gitlab.freedesktop.org/wlroots/wlr-protocols
[scope]: https://github.com/labwc/labwc-scope#readme
[wiki]: https://github.com/labwc/labwc/wiki
[getting-started]: https://labwc.github.io/getting-started.html
[integration]: https://labwc.github.io/integration.html

[rc.xml]: docs/rc.xml
[rc.xml.all]: docs/rc.xml.all
[menu.xml]: docs/menu.xml
[autostart]: docs/autostart
[environment]: docs/environment
[themerc]: docs/themerc

[labwc(1)]: https://labwc.github.io/labwc.1.html
[labwc-config(5)]: https://labwc.github.io/labwc-config.5.html
[labwc-menu(5)]: https://labwc.github.io/labwc-menu.5.html
[labwc-environment(5)]: https://labwc.github.io/labwc-environment.5.html
[labwc-theme(5)]: https://labwc.github.io/labwc-theme.5.html
[labwc-actions(5)]: https://labwc.github.io/labwc-actions.5.html
[xkeyboard-config(7)]: https://manpages.debian.org/testing/xkb-data/xkeyboard-config.7.en.html

[grim]: https://github.com/emersion/grim
[wf-recorder]: https://github.com/ammen99/wf-recorder
[swaybg]: https://github.com/swaywm/swaybg
[waybar]: https://github.com/Alexays/Waybar
[yambar]: https://codeberg.org/dnkl/yambar
[lavalauncher]: https://sr.ht/~leon_plickat/LavaLauncher
[sfwbar]: https://github.com/LBCrion/sfwbar
[bemenu]: https://github.com/Cloudef/bemenu
[fuzzel]: https://codeberg.org/dnkl/fuzzel
[wofi]: https://hg.sr.ht/~scoopta/wofi
[wlopm]: https://git.sr.ht/~leon_plickat/wlopm
[kanshi]: https://sr.ht/~emersion/kanshi/
[wlr-randr]: https://sr.ht/~emersion/wlr-randr/
[swaylock]: https://github.com/swaywm/swaylock

[Video (0:18)]: https://github.com/labwc/labwc/pull/76#issue-1028182472
[Video (1:10)]: https://youtu.be/AU_M3n_FS-E
[Video (3:42)]: https://youtu.be/rE1bQjSVJzg
