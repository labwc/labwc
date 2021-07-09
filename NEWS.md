# NEWS

This file contains significant user-visible changes for each version.
For full changelog, use `git log`

## 0.3.0 (2021-06-28)

Compile with wlroots 0.14.0

- Add config options `<focus><followMouse>` and `<focus><raiseOnFocus>`
  (provided-by: Mikhail Kshevetskiy)
- Do not use Clearlooks-3.4 theme by default, just use built-in theme
- Fix bug which triggered Qt application segfault

## 0.2.0 (2021-04-15)

Compile with wlroots 0.13.0

- Support wlr-output-management protcol for setting output position, scale
  and orientation with kanshi or similar
- Support server side decoration rounded corners
- Change built-in theme to match default GTK style
- Add labwc-environment(5)
- Call `wlr_output_enable_adaptive_sync()` if `LABWC_ADAPTIVE_SYNC` set

## 0.1.0 (2021-03-05)

Compile with wlroots 0.12.0 and wayland-server >=1.16

- Support xdg-shell and optionally xwayland-shell
- Show xbm buttons for maximize, iconify and close
- Support layer-shell protocol (partial)
- Support damage tracking to reduce CPU usage
- Support very basic root-menu implementation
- Re-load config and theme on SIGHUP
- Support simple configuration to auto-start applications, set
  environment variables and specify theme, font and keybinds.
- Support some basic theme settings for window borders and title bars
- Support basic actions including Execute, Exit, NextWindow, Reconfigure and 
  ShowMenu

