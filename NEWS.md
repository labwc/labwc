# Introduction

This file contains significant user-visible changes for each version.
For full changelog, use `git log`.

The format is based on [Keep a Changelog]

# Summary of Releases

| Date       | Release notes | wlroots version | lines-of-code |
|------------|---------------|-----------------|---------------|
| 2023-05-08 | [0.6.3]       | 0.16.2          | 13050         |
| 2023-03-20 | [0.6.2]       | 0.16.2          | 12157         |
| 2023-01-29 | [0.6.1]       | 0.16.1          | 11828         |
| 2022-11-17 | [0.6.0]       | 0.16.0          | 10830         |
| 2022-07-15 | [0.5.3]       | 0.15.1          | 9216          |
| 2022-05-17 | [0.5.2]       | 0.15.1          | 8829          |
| 2022-04-08 | [0.5.1]       | 0.15.1          | 8829          |
| 2022-02-18 | [0.5.0]       | 0.15.1          | 8766          |
| 2021-12-31 | [0.4.0]       | 0.15.0          | 8159          |
| 2021-06-28 | [0.3.0]       | 0.14.0          | 5051          |
| 2021-04-15 | [0.2.0]       | 0.13.0          | 5011          |
| 2021-03-05 | [0.1.0]       | 0.12.0          | 4627          |

## 0.6.3 - 2023-05-08

### Added

- Add `focus.followMouseRequiresMovement` to allow a stricter
  focus-what-is-under-the-cursor configuration. #862

- Support window-rules including properties and on-first-map actions.
  Any actions in labwc-actions(5) can be used. Only 'serverDecoration'
  has been added as a property so far. Example config:

      <windowRules>
        <windowRule identifier="some-application">
          <action name="Maximize"/>
        </windowRule>
        <windowRule identifier="foo*" serverDecoration="yes|no"/>
      </windowRules>

- Support configuration of window switcher field definitions.
  Issues #852 #855 #879

      <windowSwitcher show="yes" preview="yes" outlines="yes">
        <fields>
          <field content="type" width="25%" />
          <field content="app_id" width="25%" />
          <field content="title" width="50%" />
        </fields>
      </windowSwitcher>

- Add actions:
    - 'Lower' Written-by: @jech
    - 'Maximize'
- Support ext-session-lock protocol. Helped-by: @heroin-moose
- Handle XWayland unmanaged surface requests for 'activate' and
  'override-redirect'. Fixes: #874
- Add config support for scroll-factor. Fixes #846
- Support 'follow' attribute for SendToDesktop action. Fixes #841

### Fixed

- Fix adaptive sync configuration. Helped-by: @heroin-moose #642
- Ignore SIGPIPE to fix crash caused by Wayland clients requesting X11
  clipboard but closing the read-fd before/while the X11 clipboard is
  being written to. Fixes #890
- Ellipsize on-screen-display text
- Validate PID before activating XWayland unmanaged surfaces to check that
  the surface trying to grab focus is actually a child of the topmost
  mapped window.
- Respect cursor constraint hints when cursor movement occurs after
  unlocking the pointer. Written-by: @FuzzyQuills Fixes #872
- Fix invisible cursor on startup and output loss/restore.
  Reported-by: @Flrian Fixes #820
- Fix decoration protocol implementation
    - Respect earlier decoration negotiation results via the
      xdg-decoration protocol. Previously setting `<decoration>` to
      `client` would cause applications which prefer server side
      decorations to not have any decorations at all.
      Fixes #297 #831
    - Handle results of kde-server-decoration negotiations
- Fix `<focus><followMouse>` cursor glitches and issues with focus
  switching via Alt-Tab. Issue #830 #849

### Changed

- Make `<windowSwitcher>` a toplevel element rather than a child of
  `<core>`
- Default to follow="true" for SendToDesktop action as per Openbox 3.6
  specification.

## 0.6.2 - 2023-03-20

This release contains refactoring and simplification relating to
view-output association and xdg/xwayland configure/map events.

Unless otherwise stated all contributions are by the core-devs
(@Consolatis, @jlindgren90 and @johanmalm).

### Added

- Add config option `<core><windowSwitcher show="no" />` to hide
  windowSwitcher (also known as On Screen Display) when switching windows.
- Enable config option `<core><windowSwitcher preview="" />` by default.
- Add ToggleKeybinds action to disable/enable all keybinds (other than
  ToggleKeybinds itself). This can be used to better control Virtual
  Machines, VNC clients, nested compositors or similar. (#738 and #810)
- Implement cursor constraints (Written-by: @Ph42oN) and lock confinement.
- Support xdg-activation protocol to allow applications to activate
  themselves (e.g. raise to the top and get keyboard focus) if they
  provide a valid `xdg_activation token`.
- Allow clearing key/mouse bindings by using the 'None' action. This
  enables the use of `<default />` and then selectively removing keybinds.
  For example the following could be used to allow using A-Left/Right with
  Firefox.

      <keyboard>
        <default/>
        <keybind key="A-Left"><action name="None" /></keybind>
        <keybind key="A-Right"><action name="None" /></keybind>
      </keyboard>

### Fixed

- Prevent cursor based region-snapping when starting a move with Alt-Left.
  If region-snapping is wanted in this situation, just press the modifier
  again. (#761)
- Prevent rare crash due to layering move/resize/menu operations. (#817)
- Fully reset config default values on Reconfigure if not set in config
  file.
- Fix visual glitch when resizing xfce4-terminal from left edge caused by
  windows not accepting their request size exactly.
- Fix issue with havoc not having a valid size on map.
- Save `natural_geometry.x/y` with initially maximized xdg-view to fix an
  issue where, if Thunar was started maximized, it would un-maximize to
  the top-left corner rather than the center.

### Changed

- Change config option `<cycleView*>` to `<windowSwitcher>`.
  Use `<core><windowSwitcher show="yes" preview="no" outlines="yes" />`
  instead of:

      <core>
        <cycleViewOSD>yes</cycleViewOSD>
        <cycleViewOutlines>yes</cycleViewOutlines>
        <cycleViewPreview>yes</cycleViewPreview>
      </core>

## 0.6.1 - 2023-01-29

As usual, this release contains lots of refactoring and bug fixes with
particular thanks going to @Consolatis, @jlindgren90, @bi4k8, @Flrian and
@Joshua-Ashton.

### Added

- Add `<regions>` config option allowing the definition of regions to which
  windows can be snapped by keeping a keyboard modifier pressed while dragging
  or by using the SnapToRegion action. Written-by: @Consolatis
- Add `<Kill>` action to send SIGTERM to a client process. Written-by: @bi4k8
- Add config option `<core><reuseOutputMode>` to support flicker free boot
  (issue #724). Written-by: @Consolatis
- Enable single-pixel-buffer-v1
- Support theme setting override by reading `<config-dir>/themerc-override`
- Scale down SSD button icons if necessary to allow using larger ones for high
  and mixed DPI usecases. Issue #609. Written-by: @Consolatis
- Handle client request for layer-change
- Support setting color of client menu buttons. Written-by: @Flrian
- Dynamically adjust menu width based on widest item. Written-by: @Consolatis
- Add theme options menu.width.{min,max} and menu.items.padding.{x,y}

### Fixed

- Scale cursor correctly at startup and on output scale-change.
  Written-by: @bi4k8
- Release layer tree when releasing output. Written-by: @yuanye
- Ensure natural geometry is restored when no outputs available.
  Reported-by: @Flrian
- Fixes memory leaks and prevent crashes associated with missing outputs
  Thanks to @Consolatis.
- Update translations for new client menus strings. Thanks-to: @01micko and
  @ersen0
- On un-fullscreen, restore SSD before applying previous geometry to avoid
  rendering offscreen in some instances. Written-by: @Consolatis
- Allow snapping to the same edge. Thanks-to: @Consolatis and @Flrian
- Send enter event when new layer surface appears under pointer. Issue #667
- Prevent re-focus for always-on-top views when switching workspaces.
  Written-by: @Consolatis
- Make sure a default libinput category always exists to avoid devices not
  being configured is some insances. Written-by: @jlindgren90
- Update cursor if it is within the OSD area when OSD appears/disappears.
  Written-by: @bi4k8
- Provide generic parsing of XML action arguments to enable the use of the
  `direction` argument in menu entries. Written-by: @Consolatis
- Fix SSD margin computation. Written-by: @jlindgren90
- Hide SSD decorations for fullscreen views to avoid rendering them on
  adjacent outputs. Written-by: @jlindgren90
- Set inactive window button color correctly. Written-by: @ScarcelyThere
- Fix positioning of initially-maximized XWayland views.
  Written-by: @jlindgren90
- Check for modifiers when merging mousebinds. Issue #630.
- Handle layer-shell exclusive and on-demand keyboard-interactivity
  correctly, and thus support xfce4-panel better. Issues #704 and #725.
- Only overwrite wlroots's automatic layout when necessary.

### Changed

- Filter out wp_drm_lease_device from Xwayland to avoid Electron apps such as
  VS Code and Discord lagging over time. Issue #553. Written-by: @Joshua-Ashton
- Do not switch output on SnapToEdge if view is maximized. Written-by: @Flrian

## 0.6.0 - 2022-11-17

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

- Set environment variable LABWC_PID to the pid of the compositor so that
  SIGHUP and SIGTERM can be sent to specific instances.
- Add command line options --exit and --reconfigure.
- Support setting keyboard repeat and delay at runtime. Written-by: @bi4k8
- Add support for mouse-wheel bindings. Set default bindings to switch
  workspaces when scrolling on the desktop.  Written-by: @Arnaudv6
- Implement key repeat for keybindings. Written-by: @jlindgren90
- Support smooth scroll and horizontal scroll. Written-by: @bi4k8
- Implement virtual keyboard and pointer protocols, enabling the use of
  clients such as wtype and wayvnc. Written-by: @Joshua-Ashton
- Add github workflow CI including Debian, FreeBSD, Arch and Void,
  including a build without xwayland.
- Support keybind "None" action to clear other actions for a particular
  keybind context. Written-by: @jlindgren90
- Support font slant (itliacs) and weight (bold). Written-by: @jlindgren90
- Support `<default />` mousebinds to load default mousebinds and provide
  a way to keep config files simpler whilst allowing user specific binds.
  Issue #416. Written-by: @Consolatis
- Add config option `<core><cycleViewOutlines>` to enable/disable preview
  of outlines. Written-by: @Flrian
- Render submenu arrows
- Allow highest level menu definitions - typically used for root-menu and
  client-menu - to be defined without label attritube, for example like this:
  `<openbox_menu><menu id="root-menu">...</menu></openbox>`. Issue #472
- Allow xdg-desktop-portal-wlr to work out of the box by initializing dbus
  and systemd activation environment. This enables for example OBS Studio
  to work with no user configuration. If systemd or dbus is not available
  the environment update will fail gracefully. PR #461
  Written-by: @Joshua-Ashton and @Consolatis
- Workspaces. Written-by: @Consolatis
- presentation-time protocol
- Native language support for client-menus. Written-by: @01micko
- Touch support. Written-by: @bi4k8
- drm_lease_v1 for VR to work and leasing of desktop displays.
  Written-by: Joshua Ashton
- ToggleAlwaysOnTop action. Written-by: @Consolatis
- Command line option -C to specify config directory
- Theme options osd.border.color and osd.border.width. Written-by: @Consolatis
- Menu `<separator />` and associated theme options:
  menu.separator.width, menu.separator.padding.width,
  menu.separator.padding.height and menu.separator.color
- Adjust maximized and tiled windows according to usable_area taking
  into account exclusive layer-shell clients. Written-by: @Consolatis
- Restore natural geometry when moving tiled/maximized window
  Fixes #391. Written-by: @Consolatis
- Improve action implementation to take a list of arguments in preperation
  for actions with multiple arguments. Written-by: @Consolatis

### Fixed

- Remove unwanted gap when initially (on map) positioning windows larger
  than output usable area (issue #403).
- Prevent setting cursor icon on drag. Written-by: @Consolatis (issue #549)
- Fix bugs relating to sending matching pairs of press and release
  keycodes to clients when using keybinds. Also fix related key-repeat
  bug. (Issue #510)
- Fix wlr_output_cursor initialization bug on new output.
  Written-by: @jlindgren90
- Show correct cursor for resize action triggered by keybind.
  Written-by: @jlindgren
- Fix bug which manifest itself when keeping button pressed in GTK3 menu
  and firefox context menu. Written-by: @jlindgren90
- Enable tap be default on non-touch devices (which some laptop trackpads
  apparently are)
- Handle missing cursor theme (issue #246). Written-by: @Consolatis
- Fix various surface syncronization, stacking, positioning and focus
  issues, including those related to both xwayland, scroll/drag events
  and also #526 #483
- On first map, do not center xwayland views with explicitly specified
  position. Written-by: @jlindgren90
- Give keyboard focus back to topmost mapped view when unmapping topmost
  xwayland unmanaged surfaces, such as dmenu. Written-by: @Consolatis.
- Fix mousebind ordering and replace earlier mousebinds by later ones
  Written-by: @Consolatis
- Fix various bugs associated with destroying/disabling outputs, including
  issue #497
- Hide Alt-Tab switcher when canceling via Escape. @jlindgren90
- (Re)set seat when xwayland is ready (because wlroots reset the seat
  assigned to xwayland to NULL whenever Xwayland terminates).
  Issues #166 #444. Written-by: @Consolatis. Helped-by: @droc12345
- Increase File Descriptor (FD) limit to max because a compositor has to
  handle many: client connections, DMA-BUFs, wl_data_device pipes and so on.
  Fixes client freeze/crashes (swaywm/sway#6642). Written-by: @Joshua-Ashton
- Fix crash when creating a cursor constraint and there is no currently
  focused view.
- Gracefully handle dying client during interactive move.
  Written-by: @Consolatis
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
- Support `<menu><item><action name="Execute"><execute>`
  `<exectue>` is a deprecated name for `<command>`, but is supported for
  backward compatibility with old menu-generators.
- Keep xwayland-shell SSD state on unmap/map cycle.
  Written-by: @Consolatis
- Prevent segfault on missing direction arguments. Reported-by: @flrian
- Fix keybind insertion order to restore intended behavior of keybinds
  set by `<default />`. Written-by: @Consolatis
- Ensure client-menu actions are always applied on window they belong to
  This fixes #380. Written-by: @Consolatis
- Keep window margin in sync when toggling decorations.
  Written-by: @Consolatis
- Fix handling of client-initiated configure requests.
  Written-by: @jlindgren90
- Always react to new output configuration. Reported-by @heroin-moose and
  Written-by: @Consolatis
- Fix bug in environment variable expansion by allowing underscores to be
  part of the variable names. Issue #439
- Fix parsing bug of adaptiveSync setting and test for support

### Changed

- src/config/rcxml.c: distinguish between no and unknown font places so
  that `<font>` with no `place` attribute can be added after other font
  elements without over-writing their values. Written-by: @bi4k8
- theme: change window.label.text.justify default to center
- Redefine the SSD "Title" context to cover the whole Titlebar area except
  the parts occupied by buttons. This allows "Drag" and "DoubleClick"
  actions to be de-coupled from buttons. As a result, "Drag" and
  "DoubleClick" actions previously defined against "TitleBar" should now
  come under the "Title" context, for example:
  `<mousebind button="Left" action="Drag"><action name="Move"/></mousebind>`
- Remove default alt-escape keybind for Exit because too many people have
  exited the compositor by mistake trying to get out of alt-tab cycling
  or similar.

## [0.5.3] - 2022-07-15

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
[0.6.3]: https://github.com/labwc/labwc/releases/tag/0.6.3
[0.6.2]: https://github.com/labwc/labwc/releases/tag/0.6.2
[0.6.1]: https://github.com/labwc/labwc/releases/tag/0.6.1
[0.6.0]: https://github.com/labwc/labwc/releases/tag/0.6.0
[0.5.3]: https://github.com/labwc/labwc/releases/tag/0.5.3
[0.5.2]: https://github.com/labwc/labwc/releases/tag/0.5.2
[0.5.1]: https://github.com/labwc/labwc/releases/tag/0.5.1
[0.5.0]: https://github.com/labwc/labwc/releases/tag/0.5.0
[0.4.0]: https://github.com/labwc/labwc/releases/tag/0.4.0
[0.3.0]: https://github.com/labwc/labwc/releases/tag/0.3.0
[0.2.0]: https://github.com/labwc/labwc/releases/tag/0.2.0
[0.1.0]: https://github.com/labwc/labwc/releases/tag/0.1.0

