# NEWS

This file contains significant user-visible changes for each version.
For full changelog, use `git log`

## next release

- The config option `<lab><xdg_shell_server_side_deco>` has changed to
  `<core><decoration>`
- Support `wlr_input_inhibit_unstable_v1 protocol`. This enables swaylock
  to be run. Written-by: @telent
- Support `wlr_foreign_toplevel protocol`. This enables controlling
  windows from panels such as waybar.
- Support fullscreen mode.
- Support drag-and-drop. Written-by: @ARDiDo
- Support some libinput configuration. Written-by: @ARDiDo
- Add the following config options:
    - Load default keybinds on `<keyboard><default />`
    - Specify distance between views and output edges with `<core><gap>`
    - Set menu item font with `<theme><font place="MenuItem">`
    - Allow `<theme><font>` without place="" attribute, thus enabling
      simpler config files
- Support for primary selection. Written-by: @telent
- Support 'alt-tab' on screen display when cycling between windows
- Add theme options to set buttons colors individually (for iconify, close
  and maximize)
- Show application title in window decoration title bar
- Handle double click on window decoration title bar
- Support a 'resize-edges' area that is wider than than the visible
  window decoration. This makes it easier to grab edges to resize
  windows.
- Add window actions 'MoveToEdge', 'ToggleMaximize', 'Close', 'Iconfiy',
  'ToggleDecorations', 'ToggleFullscreen'
- Add labwc.desktop for display managers
- Take into account exclusive areas of clients using layer-shell (such
  as panels) when maximizing windows
- Handle alt + mouse button to move/resize windows
- Catch ctrl-alt-F1 to F12 to switch tty

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

