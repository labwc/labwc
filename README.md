# labwc

A light-weight openbox alternative for Wayland.

This software is in early development.

## Dependencies

- wlroots (>=0.10.0)
- wayland-protocols
- xwayland

## Aim

[x] Support xwayland  
[ ] Support some of openbox's rc.xml  
[ ] Support openbox themes  
[ ] Support layer-shell's background layer  

## Influenced by

- [sway](https://github.com/swaywm/sway)
- [cage](https://www.hjdskes.nl/blog/cage-01/)
- [wio](https://wio-project.org/)
- [rootston](https://github.com/swaywm/rootston)
- [openbox](https://github.com/danakj/openbox)
- [i3](https://github.com/i3/i3)
- [dwm](https://dwm.suckless.org)

## Alternatives

The following were considered before choosing wlroots:

- [QtWayland](https://github.com/qt/qtwayland), [grefsen](https://github.com/ec1oud/grefsen)
- [Mir](https://mir-server.io), [egmde](https://github.com/AlanGriffiths/egmde)

## Configuration

### Keyboard Shortcuts

We will support rc.xml keybinds, but for the time being:

```
Alt+Escape  Exit labwc
Alt+F2      Cycle windows
Alt+F3      Launch dmenu
```

### Keyboard Layout

Set environment variable `XKB_DEFAULT_LAYOUT` for your keyboard layout, for
example `gb`. Read `xkeyboard-config(7)` for details.

## Integration

- Use grim for scrots

## Build

### Arch Linux

    sudo pacman -S wlroots
    git clone https://github.com/johanmalm/labwc
    cd labwc
    meson build
    ninja -C build

### Debian

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

## Debug

To enable ASAN and UBSAN, run meson with `-Db_sanitize=address,undefined`
