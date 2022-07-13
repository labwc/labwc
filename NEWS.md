# Introduction

This file contains significant user-visible changes for each version.
For full changelog, use `git log`.

The format is based on [Keep a Changelog]

On the `master` branch we have moved across to the wlroots scene-graph
API and as a result there is likey to be a period of instability and
regression. A `v0.5` branch has been created as a stable pre-scene-graph
branch, but it is not envisaged that new major features will be added on
that branch.

# Summary of Releases

| Date       | Release notes | wlroots version | lines-of-code |
|------------|---------------|-----------------|---------------|
| 2022-05-17 | [0.5.2]       | 0.15.1          | 8829          |
| 2022-04-08 | [0.5.1]       | 0.15.1          | 8829          |
| 2022-02-18 | [0.5.0]       | 0.15.1          | 8766          |
| 2021-12-31 | [0.4.0]       | 0.15.0          | 8159          |
| 2021-06-28 | [0.3.0]       | 0.14.0          | 5051          |
| 2021-04-15 | [0.2.0]       | 0.13.0          | 5011          |
| 2021-03-05 | [0.1.0]       | 0.12.0          | 4627          |

## 0.6.0 - unreleased

This release contains significant refactoring to use the wlroots
scene-graph API. This touches many areas of the code, particularly
rendering, server-side-decoration, the layer-shell implementation and the
menu. Many thanks to @Consolatis for doing most of the heavy lifting with
this.

Noteworthy, related changes include:

- The use of a buffer implementation instead of using wlr_texture. It
  handles both images and fonts, and scales according to output scale.
- The use of node-descriptors to assign roles to wlr_scene_nodes in order
  to simplify the code.
- Improving the "Debug" action to print scene-graph trees

A large number of bugs and regressions have been fixed following the
re-factoring, too many to list here, but we are grateful to all who have
reported, tested and fixed issues. Particular mentions go to @bi4k8,
@flrian, @heroin-moose, @jlindgren90, @Joshua-Ashton, @01micko and @skerit

### Added

- Workspaces. Written-by: @Consolatis
- presentation-time protocol
- Native language support for client-menus. Written-by: @01micko
- Touch support. Written-by: @bi4k8
- drm_lease_v1 for VR to work and leasing of desktop displays.
  Written-by: Joshua Ashton
- ToggleAlwaysOnTop action. Written-by: @Consolatis
- Command line option -C to specify config directory
- Theme options osd.border.color and osd.border.width. Written-by: @Consolatis
- Menu <separator /> and associated theme options:
  menu.separator.width, menu.separator.padding.width,
  menu.separator.padding.height and menu.separator.color
- Adjust maximized and tiled windows according to usable_area taking
  into account exclusive layer-shell clients. Written-by: @Consolatis
- Restore natural geometry when moving tiled/maximized window
  Fixes #391. Written-by: @Consolatis
- Improve action implementation to take a list of arguments in preperation
  for actions with multiple arguments. Written-by: @Consolatis

### Fixed

- Dynamically adjust server-side-deccoration invisible resize areas based
  on usable_area to ensure that cursor events are sent to clients such as
  panels in preference to grabbing window edges. Fixes #265.
  Written-by: @Consolatis
- Always position submenus inside output extents. Fixes #276
  Written-by: @Consolatis
- Do not crash when changing TTY. Written-by: @bi4k8
- Set wlroots.wrap to a specific commit rather than master because it
  enables labwc commits to be checked out and build without manually
  having to find the right wlroots commit if there are upstream breaking
  changes.
- Increase accuracy of window center-alignment, taking into account
  usable_area and window decoration. Also, top/left align if window is
  bigger than usable area.
- Handle view-destruction during alt-tab cycling.
  Written-by: @Joshua-Ashton
- Survive all outputs being disabled
- Check that double-clicks are on the same window. Written-by: yizixiao
- Set xdg-shell window position before maximize on first map so that the
  unmaximized geometry is known when started in maximized mode.
  Fixes issue #305. Reported-by: @01micko
- Support <menu><item><action name="Execute"><execute>
  <exectue> is a deprecated name for <command>, but is supported for
  backward compatibility with old menu-generators.
- Keep xwayland-shell SSD state on unmap/map cycle.
  Written-by: @Consolatis
- Prevent segfault on missing direction arguments. Reported-by: @flrian
- Fix keybind insertion order to restore intended behavior of keybinds
  set by <default />. Written-by: @Consolatis
- Ensure client-menu actions are always applied on window they belong to
  This fixes #380. Written-by: @Consolatis
- Keep window margin in sync when toggling decorations.
  Written-by: @Consolatis
- Fix handling of client-initiated configure requests.
  Written-by: @jlindgren90
- Always react to new output configuration. Reported-by @heroin-moose and
  Written-by: @Consolatis

### Changed

- theme: change window.label.text.justify default to center

## [0.5.3] - unreleased

### Added

- wlr-output-power-management protocol to enable clients such as wlopm
  Written-by: @bi4k8

### Fixed

- Call foreign-toplevel-destroy when unmapping xwayland surfaces because
  some xwayland clients leave unmapped child views around. Although
  handle_destroy() is not called for these, we have to call
  foreign-toplevel-destroy to avoid clients such as panels incorrecly
  showing them.
- Handle xwayland set_override_redirect events to fix weird behaviour with
  gitk menus and rofi.
- Re-focus parent surface on unmapping xwayland unmanaged surfaces
  Fixes #352 relating to JetBrains and Intellij focus issues
  Written-by: Jelle De Loecker
- Do not segfault on missing drag icon. Written-by: @Consolatis
- Fix windows irratically sticking to edges during move/resize.
  Fixes issues #331 and #309

## [0.5.2] - 2022-05-17

This is a minor bugfix release mostly to ease packaging.

### Fixed

- Properly use system provided wlroots. Written-by: @eli-schwartz

## [0.5.1] - 2022-04-08

### Added

- Honour size increments from WM_SIZE_HINTS, for example to allow
  xwayland terminal emulators to be resized to a width/height evenly
  divisible by the cell size. Written-by: @jlindgren90
- Implement cursor input for overlay popups. Written-by: @Consolatis

### Fixed

- Do not raise xwayland windows when deactivating (issue #270).
  Written-by: @Consolatis
- Restore drag mouse-bindings and proper double-click (issues #258 and
  #259). Written-by: @Consolatis
- Implement cursor input for unmanaged xwayland surfaces outside their
  parent view. Without this menus extending outside the main application
  window do not receive mouse input. Written-by: @jlindgren90
- Allow dragging scrollbar or selecting text even when moving cursor
  outside of the window (issue #241). Written-by: @Consolatis
- Fix positioning of xwayland views with multiple queued configure
  events. Written-by: @Consolatis
- Force a pointer enter event on the surface below the cursor when
  cycling views (issue #162). Written-by: @Consolatis
- Fix qt application crash on touchpad scroll (issue #225).
  Written-by: @Consolatis

## [0.5.0] - 2022-02-18

As usual, this release contains a bunch of fixes and improvements, of
which the most notable feature-type changes are listed below. A big
thank you to @ARDiDo, @Consolatis and @jlindgren90 for much of the hard
work.

### Added

- Render overlay layer popups to support sfwbar (issue #239)
- Support HiDPI on-screen-display images for outputs with different scales
- Reload environment variables on SIGHUP (issue #227)
- Add client menu
- Allow applications to start in fullscreen
- Add config option `<core><cycleViewPreview>` to preview the contents
  of each view when cycling through them (for example using alt-tab).
- Allow mouse movements to trigger SnapToEdge. When dragging a view, move
  the cursor outside an output to snap in that direction.
- Unmaximize on Move
- Support wlroots environment variable `WLR_{WL,X11}_OUTPUTS` for running
  in running nested in X11 or a wlroots compositor.
- Support pointer gestures (pinch/swipe)
- Adjust views to account for output layout changes

### Changed

This release contains the following two breaking changes:

- Disabling outputs now causes views to be re-arranged, so in the
  context of idle system power management (for example when using
  swaylock), it is no longer suitable to call wlr-randr {--off,--on}
  to enable/disable outputs.
- The "Drag" mouse-event and the unmaximize-on-move feature require
  slightly different `<mousebind>` settings to feel natural, so suggest
  updating any local `rc.xml` settings in accordance with
  `docs/rc.xml.all`

## [0.4.0] - 2021-12-31

Compile with wlroots 0.15.0

This release contains lots of internal changes, fixes and  new features.
A big thank you goes out to @ARDiDo, @bi4k8, @Joshua-Ashton,
@jlindgren90, @Consolatis, @telent and @apbryan. The most notable
feature-type changes are listed below.

### Added

- Add support for the following wayland protocols:
    - `pointer_constraints` and `relative_pointer` - mostly for gaming.
      Written-by: @Joshua-Ashton
    - `viewporter` - needed for some games to fake modesets.
      Written-by: @Joshua-Ashton
    - `wlr_input_inhibit`. This enables swaylock to be run.
      Written-by: @telent
    - `wlr_foreign_toplevel`. This enables controlling windows from clients
      such as waybar.
    - `idle` and `idle_inhibit` (Written-by: @ARDiDo)
- Support fullscreen mode.
- Support drag-and-drop. Written-by: @ARDiDo
- Add the following config options:
    - Load default keybinds on `<keyboard><default />`
    - `<keyboard><repeatRate>` and `<keyboard><repeatDelay>`
    - Specify distance between views and output edges with `<core><gap>`
    - `<core><adaptiveSync>`
    - Set menu item font with `<theme><font place="MenuItem">`
    - Allow `<theme><font>` without place="" attribute, thus enabling
      simpler config files
    - Support `<mousebind>` with `contexts` (e.g. `TitleBar`, `Left`,
      `TLCorner`, `Frame`), `buttons` (e.g. `left`, `right`), and
      `mouse actions` (e.g. `Press`, `DoubleClick`). Modifier keys are
      also supported to handle configurations such as `alt` + mouse button
      to move/resize windows. (Written-by: @bi4k8, @apbryan)
    - `<libinput>` configuration. Written-by: @ARDiDo
    - `<resistance><screenEdgeStrength>`
- Support for primary selection. Written-by: @telent
- Support 'alt-tab' on screen display when cycling between windows
  including going backwards by pressing `shift` (Written-by: @Joshua-Ashton)
  and cancelling with `escape` (Written-by: @jlindgren90)
- Add the following theme options:
    - set buttons colors individually (for iconify, close and maximize)
    - `window.(in)active.label.text.color`
    - `window.label.text.justify`
    - OSD colors
- Show application title in window decoration title bar
- Handle double click on window decoration title bar
- Support a 'resize-edges' area that is wider than than the visible
  window decoration. This makes it easier to grab edges to resize
  windows.
- Add window actions 'MoveToEdge', 'ToggleMaximize', 'Close', 'Iconify',
  'ToggleDecorations', 'ToggleFullscreen', 'SnapToEdge', 'Focus', 'Raise',
  'Move', 'MoveToEdge', 'Resize', 'PreviousWindow', 'ShowMenu'
- Add labwc.desktop for display managers
- layer-shell:
    - Take into account exclusive areas of clients (such as panels) when
      maximizing windows
    - Support popups
- Handle xwayland `set_decorations` and xdg-shell-decoration requests.
  Written-by: @Joshua-Ashton
- Handle view min/max size better, including xwayland hint support.
  Written-by: @Joshua-Ashton
- Handle xwayland move/resize events. Written-by: @Joshua-Ashton
- Support audio and monitor-brightness keys by default
- Catch ctrl-alt-F1 to F12 to switch tty
- Support `XCURSOR_THEME` and `XCURSOR_SIZE` environment variables
- Support submenus including inline definitions

### Changed

- The config option `<lab><xdg_shell_server_side_deco>` has changed to
  `<core><decoration>` (breaking change)

## [0.3.0] - 2021-06-28

Compile with wlroots 0.14.0

### Added

- Add config options `<focus><followMouse>` and `<focus><raiseOnFocus>`
  (provided-by: Mikhail Kshevetskiy)
- Do not use Clearlooks-3.4 theme by default, just use built-in theme
- Fix bug which triggered Qt application segfault

## [0.2.0] - 2021-04-15

Compile with wlroots 0.13.0

### Added

- Support wlr-output-management protcol for setting output position, scale
  and orientation with kanshi or similar
- Support server side decoration rounded corners
- Change built-in theme to match default GTK style
- Add labwc-environment(5)
- Call `wlr_output_enable_adaptive_sync()` if `LABWC_ADAPTIVE_SYNC` set

## [0.1.0] - 2021-03-05

Compile with wlroots 0.12.0 and wayland-server >=1.16

### Added

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

[Keep a Changelog]: https://keepachangelog.com/en/1.0.0/
[0.5.2]: https://github.com/labwc/labwc/releases/tag/0.5.2
[0.5.1]: https://github.com/labwc/labwc/releases/tag/0.5.1
[0.5.0]: https://github.com/labwc/labwc/releases/tag/0.5.0
[0.4.0]: https://github.com/labwc/labwc/releases/tag/0.4.0
[0.3.0]: https://github.com/labwc/labwc/releases/tag/0.3.0
[0.2.0]: https://github.com/labwc/labwc/releases/tag/0.2.0
[0.1.0]: https://github.com/labwc/labwc/releases/tag/0.1.0

