# NEWS

This file contains significant user-visible changes for each version.
For full changelog, use `git log`

## 0.3.0 (2021-06-28)

Compile with wlroots 0.14.0

- add config options <focus><followMouse> and <focus><raiseOnFocus>
  (provided-by: Mikhail Kshevetskiy)
- stop trying to use Clearlooks-3.4 theme by default, just use built-in theme
- fix Qt application segfault

## 0.2.0 (2021-04-15)

Compile with wlroots 0.13.0

- support wlr-output-management protcol for setting output position, scale
  and orientation with kanshi or similar
- config:
  - add `<theme><cornerRadius>` for window decoration rounded corners
- theme:
  - update built-in colors to match default GTK style
  - add window.(in)active.border.color
  - add border.width
- add labwc-environment(5)
- call `wlr_output_enable_adaptive_sync()` if `LABWC_ADAPTIVE_SYNC` set

## 0.1.0 (2021-03-05)

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

