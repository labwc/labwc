# labwc

Labwc is a [WIP] free, stacking compositor for Wayland.

It is in early development and has the following aims:

- Be light-weight, small and fast.
- Have the look and feel of the X11 Window Manager Openbox.
- Where practicable, use other software to show wall-paper, take screenshots,
  and so on.

[Dependencies](#dependencies)
[Roadmap](#roadmap)
[Inspiration](#inspiration)
[Design](#design)
[Configuration](#configuration)
[Integration](#integration)
[Build](#build)
[Debug](#debug)

## Dependencies

Runtime dependencies include wlroots (>=0.10.0), wayland-protocols,xwayland,
libxml2, glib-2.0, cairo and pango.

## Roadmap

- [x] Support xwayland
- [x] Parse [rc.xml](data/rc.xml)
- [x] Parse [themerc](data/themerc)
- [x] Read xbm icons
- [ ] Add maximize, minimize, close buttons
- [ ] Add grip
- [ ] Create `view_impl` starting with .configure
- [ ] Support layer-shell background (e.g. using swaybg)
- [ ] Draw better alt-tab rectangle
- [ ] Try restarting and consider catching SIGHUP for --reconfigure
- [ ] Implement client-menu
- [ ] Implement root-menu

## Inspiration

Labwc has been inspired and inflenced by [openbox](https://github.com/danakj/openbox), [sway](https://github.com/swaywm/sway), [cage](https://www.hjdskes.nl/blog/cage-01/), [wio](https://wio-project.org/) and [rootston](https://github.com/swaywm/rootston)

## Design

Labwc is based on the wlroots library.

The following were considered before choosing wlroots: [QtWayland](https://github.com/qt/qtwayland), [grefsen](https://github.com/ec1oud/grefsen), [Mir](https://mir-server.io) and [egmde](https://github.com/AlanGriffiths/egmde).

## Configuration

See [rc.xml](data/rc.xml) and [themerc](data/themerc) comments for details
incl. keybinds.

## Integration

Suggested apps:

- grim - screenshots

## Build

### Arch Linux

    sudo pacman -S wlroots
    git clone https://github.com/johanmalm/labwc
    cd labwc
    meson build
    ninja -C build

### Debian

```
sudo apt install \
	build-essential \
	cmake \
	libwayland-dev \
	wayland-protocols \
	libegl1-mesa-dev \
	libgles2-mesa-dev \
	libdrm-dev libgbm-dev \
	libinput-dev \
	libxkbcommon-dev \
	libudev-dev \
	libpixman-1-dev \
	libsystemd-dev \
	libcap-dev \
	libxcb1-dev \
	libxcb-composite0-dev \
	libxcb-xfixes0-dev \
	libxcb-xinput-dev \
	libxcb-image0-dev \
	libxcb-render-util0-dev \
	libx11-xcb-dev \
	libxcb-icccm4-dev \
	freerdp2-dev \
	libwinpr2-dev \
	libpng-dev \
	libavutil-dev \
	libavcodec-dev \
	libavformat-dev \
	universal-ctags \
	xwayland

# Debian Buster has an old version of meson, so we use pip3
pip3 install --target=$HOME/bin meson

git clone https://github.com/johanmalm/labwc
cd labwc
git clone https://github.com/swaywm/wlroots subprojects/wlroots

# wlroots 0.10.0 is the last version which runs with Wayland 0.16
# (which is what Buster runs)
cd subprojects/wlroots && git checkout 0.10.0 && cd ../..

meson build
ninja -C build
```

## Debug

To enable ASAN and UBSAN, run meson with `-Db_sanitize=address,undefined`

