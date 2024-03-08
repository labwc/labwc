# labwc

<h3 align="center">[<a
href="https://labwc.github.io/">Website</a>] [<a
href="https://github.com/labwc/labwc-scope#readme">Scope</a>] [<a
href="https://web.libera.chat/gamja/?channels=#labwc">IRC&nbsp;Channel</a>] [<a
href="NEWS.md">Release&nbsp;Notes</a>]</h3>

- [1. Project Description](#1-project-description)
  - [1.1 What Is This?](#11-what-is-this)
  - [1.2 Why](#12-why)
  - [1.3 Why The Openbox Theme Specification?](#13-why-the-openbox-theme-specification)
  - [1.4 Very High Level Scope](#14-very-high-level-scope)
  - [1.5 Videos](#15-videos)
  - [1.6 Screenshot](#16-screenshot)
- [2. Build and Installation](#2-build-and-installation)
- [3. Configuration](#3-configuration)
- [4. Theming](#4-theming)
- [5. Translations](#5-translations)
- [6. Usage](#6-usage)
  - [6.1 Gaming](#61-gaming)
- [7. Integration](#7-integration)

## 1. Project Description

### 1.1 What Is This?

Labwc stands for Lab Wayland Compositor, where lab can mean any of the
following:

- sense of experimentation and treading new ground
- inspired by BunsenLabs and ArchLabs
- your favorite pet

Labwc is a [wlroots]-based window-stacking compositor for [wayland], inspired
by [openbox].

It is light-weight and independent with a focus on simply stacking windows well
and rendering some window decorations. It takes a no-bling/frills approach and
says no to features such as animations.  It relies on clients for panels,
screenshots, wallpapers and so on to create a full desktop environment.

Labwc tries to stay in keeping with [wlroots] and [sway] in terms of general
approach and coding style.

Labwc has no reliance on any particular Desktop Environment, Desktop Shell or
session. Nor does it depend on any UI toolkits such as Qt or GTK.

### 1.2 Why?

Firstly, we believe that there is a need for a simple Wayland window-stacking
compositor which strikes a balance between minimalism and bloat approximately
at the level where Window Managers like Openbox reside in the X11 domain.  Most
of the core developers are accustomed to low resource Desktop Environments such
as Mate/XFCE or standalone Window Managers such as Openbox under X11.  Labwc
aims to make a similar setup possible under Wayland, with small and independent
components rather than a large, integrated software eco-system.

Secondly, the Wayland community has achieved an amazing amount so far, and we
want to help solve the unsolved problems to make Wayland viable for more
people. We think that standardisation and de-fragmentation is a route to
greater Wayland adoption, and wanting to play our part in this, Labwc only
understands [wayland-protocols] &amp; [wlr-protocols], and it cannot be
controlled with dbus, sway/i3/custom-IPC or other technology.

Thirdly, it is important to us that scope is tightly controlled so that the
compositor matures to production quality. On the whole, we value robustness,
reliability, stability and simplicity over new features. Coming up with new
ideas and features is easy - maintaining and stabilising them is not.

Fourthly, we are of the view that a compositor should be boring in order to do
its job well. In this regard we follow in the footsteps of [metacity] which
describes itself as a "Boring window manager for the adult in you. Many window
managers are like Marshmallow Froot Loops; Metacity is like Cheerios."

Finally, we think that an elegant solution to all of this does not need feel
square and pixelated like something out of the 1990s, but should look
contemporary and enable cutting-edge performance.

### 1.3 Why The Openbox Theme Specification?

In order to avoid reinventing configuration and theme syntaxes, the [openbox]
3.6 specification is used. This does not mean that labwc is an openbox clone
but rather that configuration files will look and feel familiar.

Also, parsing GTK3+ and Qt themes for window decorations is very complicated,
so using much simpler specs such as those used by openbox and xfwm makes sense
for a compositor such as labwc, both in terms of implementation and for user
modification.

Openbox spec is somewhat of a stable standard considering how long it has
remained unchanged for and how wide-spread its adoption is by lightweight
distributions such as LXDE, LXQt, BunsenLabs, ArchLabs, Mabox and Raspbian. Some
widely used themes (for example Numix and Arc) have built-in support.

We could have invented a whole new syntax, but that's not where we want to
spend our effort.

### 1.4 Very High Level Scope

A lot of emphasis is put on code simplicity when considering features.

The main development effort is focused on producing a solid foundation for a
stacking compositor rather than adding configuration and theming options.

See [scope] for full details on implemented features.

High-level summary of items that Labwc supports:

- [x] Config files (rc.xml, autostart, environment, menu.xml)
- [x] Theme files and xbm/png/svg icons
- [x] Basic desktop and client menus
- [x] HiDPI
- [x] wlroots protocols such as `output-management`, `layer-shell` and
  `foreign-toplevel`
- [x] Optionally xwayland

### 1.5 Videos

| video link     | date        | content
| -------------- | ------------| -------
| [Video (2:48)] | 31-Oct-2022 | 0.6.0 release video
| [Video (1:10)] | 05-Aug-2021 | window gymnastics, theming and waybar
| [Video (3:42)] | 25-Feb-2021 | setting background and themes; xwayland/xdg-shell windows

### 1.6 Screenshot

The obligatory screenshot:

<a href="https://labwc.github.io/img/scrot1.png">
  <img src="https://labwc.github.io/img/scrot1-small.png">
</a><br />
<a href="https://labwc.github.io/obligatory-screenshot.html">
  <small>Screenshot description</small>
</a>

## 2. Build and Installation

To build, simply run:

    meson setup build/
    meson compile -C build/

Run-time dependencies include:

- wlroots, wayland, libinput, xkbcommon
- libxml2, cairo, pango, glib-2.0
- libpng
- librsvg >=2.46 (optional)
- xwayland, xcb (optional)

Build dependencies include:

- meson, ninja, gcc/clang
- wayland-protocols

Disable xwayland with `meson -Dxwayland=disabled build/`

For OS/distribution specific details see [wiki].

If the right version of `wlroots` is not found on the system, the build setup
will automatically download the wlroots repo. If this fallback is not desired
please use:

    meson setup --wrap-mode=nodownload build/

To enforce the supplied wlroots.wrap file, run:

    meson setup --force-fallback-for=wlroots build/

If installing after using the wlroots.wrap file, use the following to
prevent installing the wlroots headers:

    meson install --skip-subprojects -C build/

## 3. Configuration

User config files are located at `${XDG_CONFIG_HOME:-$HOME/.config/labwc/}`
with the following five files being used: [rc.xml], [menu.xml], [autostart],
[environment] and [themerc-override].

Run `labwc --reconfigure` to reload configuration and theme.

For a step-by-step initial configuration guide, see [getting-started].

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

## 5. Translations

The default window bar menu can be translated on the [weblate platform](https://translate.lxqt-project.org/projects/labwc/labwc/).

<a href="https://translate.lxqt-project.org/engage/labwc/?utm_source=widget">
<img src="https://translate.lxqt-project.org/widgets/labwc/-/labwc/multi-blue.svg" alt="Translation status" />
</a>

## 6. Usage

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

### 6.1 Gaming

Cursor confinement is supported from version `0.6.2`. If using older versions,
use a nested [gamescope] instance for gaming.  It can be added to steam via
game launch option: `gamescope -f -- %command%`.

## 7. Integration

Suggested apps to use with labwc:

- Screen shooter: [grim]
- Screen recorder: [wf-recorder]
- Background image: [swaybg]
- Panel: [waybar], [yambar], [lavalauncher], [sfwbar]
- Launchers: [bemenu], [fuzzel], [wofi]
- Output managers: [wlopm], [kanshi], [wlr-randr]
- Screen locker: [swaylock]

See [integration] for further details.

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
[metacity]: https://github.com/GNOME/metacity

[rc.xml]: docs/rc.xml.all
[menu.xml]: docs/menu.xml
[autostart]: docs/autostart
[environment]: docs/environment
[themerc-override]: docs/themerc
[themerc]: docs/themerc
[labwc-theme(5)]: https://labwc.github.io/labwc-theme.5.html

[gamescope]: https://github.com/Plagman/gamescope
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

[Video (2:48)]: https://youtu.be/guBnx18EQiA
[Video (1:10)]: https://youtu.be/AU_M3n_FS-E
[Video (3:42)]: https://youtu.be/rE1bQjSVJzg
