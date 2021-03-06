# v0.1.0 2020-03-05

Compile with wlroots 0.12.0 and wayland-server >=1.16

This first release supports the following:
- xdg-shell
- optionally xwayland-shell
- xbm buttons for maximize, iconify and close
- layer-shell protocol (partial)
- damage tracking to reduce CPU usage
- very basic root-menu implementation
- config and theme re-load on SIGHUP
- openbox style autostart and environment files
- 3 configuration options (openbox compatible)
  - `<theme><name>`
  - `<theme><font place="activewindow">`
  - `<keyboard><keybind>`
- 9 themes options (openbox compatible)
  - `window.active.title.bg.color`
  - `window.active.handle.bg.color`
  - `window.inactive.title.bg.color`
  - `window.active.button.unpressed.image.color`
  - `window.inactive.button.unpressed.image.color`
  - `menu.items.bg.color`
  - `menu.items.text.color`
  - `menu.items.active.bg.color`
  - `menu.items.active.text.color`
- 5 actions (openbox compatible)
  - `<action name="Execute"><command>`
  - `<action name="Exit">`
  - `<action name="NextWindow">`
  - `<action name="Reconfigure">`
  - `<action name="ShowMenu"><menu>`

