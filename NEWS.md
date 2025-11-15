# Introduction

This file contains significant user-visible changes for each version.
For full changelog, use `git log`.

The format is based on [Keep a Changelog]

# Summary of Releases

| Date       | All Changes   | wlroots version | lines-of-code |
|------------|---------------|-----------------|---------------|
| 2025-11-15 | [unreleased]  | 0.19.2          | 28825         |
| 2025-10-10 | [0.9.2]       | 0.19.1          | 28818         |
| 2025-08-02 | [0.9.1]       | 0.19.0          | 28605         |
| 2025-07-11 | [0.9.0]       | 0.19.0          | 28586         |
| 2025-05-02 | [0.8.4]       | 0.18.2          | 27679         |
| 2025-02-21 | [0.8.3]       | 0.18.2          | 27671         |
| 2024-12-13 | [0.8.2]       | 0.18.2          | 26298         |
| 2024-10-25 | [0.8.1]       | 0.18.1          | 25473         |
| 2024-08-16 | [0.8.0]       | 0.18.0          | 23320         |
| 2024-06-19 | [0.7.4]       | 0.17.4          | 22746         |
| 2024-06-12 | [0.7.3]       | 0.17.4          | 22731         |
| 2024-05-10 | [0.7.2]       | 0.17.3          | 21368         |
| 2024-03-01 | [0.7.1]       | 0.17.1          | 18624         |
| 2023-12-22 | [0.7.0]       | 0.17.1          | 16576         |
| 2023-11-25 | [0.6.6]       | 0.16.2          | 15796         |
| 2023-09-23 | [0.6.5]       | 0.16.2          | 14809         |
| 2023-07-14 | [0.6.4]       | 0.16.2          | 13675         |
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

[unreleased]: NEWS.md#unreleased
[0.9.2]: NEWS.md#092---2025-10-10
[0.9.1]: NEWS.md#091---2025-08-02
[0.9.0]: NEWS.md#090---2025-07-11
[0.8.4]: NEWS.md#084---2025-05-02
[0.8.3]: NEWS.md#083---2025-02-21
[0.8.2]: NEWS.md#082---2024-12-13
[0.8.1]: NEWS.md#081---2024-10-25
[0.8.0]: NEWS.md#080---2024-08-16
[0.7.4]: NEWS.md#074---2024-06-19
[0.7.3]: NEWS.md#073---2024-06-12
[0.7.2]: NEWS.md#072---2024-05-10
[0.7.1]: NEWS.md#071---2024-03-01
[0.7.0]: NEWS.md#070---2023-12-22
[0.6.6]: NEWS.md#065---2023-11-25
[0.6.5]: NEWS.md#064---2023-09-23
[0.6.4]: NEWS.md#063---2023-07-14
[0.6.3]: NEWS.md#062---2023-05-08
[0.6.2]: NEWS.md#061---2023-03-20
[0.6.1]: NEWS.md#060---2023-01-29
[0.6.0]: NEWS.md#060---2022-11-17
[0.5.3]: NEWS.md#053---2022-07-15
[0.5.2]: NEWS.md#052---2022-05-17
[0.5.1]: NEWS.md#051---2022-04-08
[0.5.0]: NEWS.md#050---2022-02-18
[0.4.0]: NEWS.md#040---2021-12-31
[0.3.0]: NEWS.md#030---2021-06-28
[0.2.0]: NEWS.md#020---2021-04-15
[0.1.0]: NEWS.md#010---2021-03-05

## Notes on wlroots-0.19

There are some regression warnings worth noting for the switch to wlroots 0.19:

- The DRM backend now destroys/recreates outputs on VT switch and in some cases
  on suspend/resume too. The reason for this change was that (i) the KMS state
  is undefined when a VT is switched away; and (ii) the previous outputs had
  issues with restoration, particularly when the output configuration had
  changed whilst switched away. This change causes two issues for users:
  - Some layer-shell clients do not re-appear on output re-connection, or may
    appear on a different output. Whilst this has always been the case, it will
    now also happen in said situations. We recommend layer-shell clients to
    handle the new-output and surface-destroy signals to achieve desired
    behaviours.
  - Some Gtk clients issue critical warnings as they assume that at least one
    output is always available. This will be fixed in `Gtk-3.24.50`. It is
    believed to be a harmless warning, but it can be avoided by running labwc
    with the environment variable `LABWC_FALLBACK_OUTPUT=NOOP-fallback` to
    temporarily create a fallback-output when the last physical display
    disconnects. [#2914] [#2939] [wlroots-4878] [gtk-8792]
- Due to a single-pixel protocol issue, `waylock` and `chayang` do not work.
  This will be fixed in `wlroots-0.19.1`. [#2943] [wlroots-5098]
- Menu item can no longer be activated in any Gtk applications with a single
  press-drag-release mouse action. For context: This is due to ambiguity in the
  specifications and contrary implementations. For example, Gtk applications are
  broken under KWin in this regard, while vice versa Qt clients are broken under
  other compositors like Weston, Mutter and labwc. It has been decided not to
  block the release due to this regression as it is an eco-system wide issue
  that has existed for a long time. [#2787]
- VR headset support is disabled when compiled with wlroots `0.19.0` to work
  around a bug on the wlroots side which is expected to be fixed in wlroots
  `0.19.1` [#2887]

With wlroots compiled with libwayland (>= 1.24.0), there is an invisible margin
preventing pointer focus on some layer-shell surfaces including those created by
Gtk. In simple words, this is because libwayland now rounds floats a bit
differently [#3099].  There is a pending fix [wlroots-5159].

[wlroots-4878]: https://gitlab.freedesktop.org/wlroots/wlroots/-/merge_requests/4878
[wlroots-5098]:https://gitlab.freedesktop.org/wlroots/wlroots/-/merge_requests/5098
[wlroots-5159]: https://gitlab.freedesktop.org/wlroots/wlroots/-/merge_requests/5159
[gtk-8792]: https://gitlab.gnome.org/GNOME/gtk/-/merge_requests/8792

## unreleased

[unreleased-commits]

### Added

- Allow the use of the `sendEventsMode` configuration option on keyboards in
  order to disable keyboard input. @cillian64 [#3208]

```xml
<libinput>
  <device category="  RPI Wired Keyboard 1">
    <sendEventsMode>no</sendEventsMode>
  </device>
</libinput>
```
- Support the following new `<windowSwitcher>` configuration options:
  -  `<osd thumbnailLabelFormat="%T">` to specify the label text in each item in
     the thumbnail style window-switcher. @elviosak [#3187]
  - `<osd output="all|pointer|keyboard">` to specify which monitor(s) to show
    the OSD(s) on. @dntxi [#3201]
- Support window-switcher OSD item click to focus window @tokyo4j [#3186]
- With the window-switcher custom field state specifiers 's' and 'S', show 's'
  for shaded window @domo141 [#2895]
- Support `xdg-dialog` protocol to enable better handling of modal dialogs @xi
  [#3134]
- labnag: add --keyboard-focus option @tokyo4j [#3120]
- Allow window switcher to temporarily unshade windows using config option
  `<windowSwitcher unshade="yes|no"/>` @Amodio @Consolatis [#3124]
- For the 'classic' style window-switcher, add the following theme options:
    - `osd.window-switcher.style-classic.item.active.border.color`
    - `osd.window-switcher.style-classic.item.active.bg.color`
  @tokyo4j [#3118]

### Fixed

- Update layer-shell client top layer visiblity on unmap instead of destroy
  because it is possible for fullscreen xwayland windows to be unmapped without
  being destroyed, and in this case the top layer visibility needs to be updated
  to unhide other layer-shell clients like panels. @jlindgren90 [#3199]
- Window-switcher fixes include:
  - Consider output transformation for percentage based widths @tokyo4j [#3177]
  - Classic theme miscalculation for osd width in percentage @tokyo4j [#3175]
  - Thumbnail theme miscalculation for item geometries @tokyo4j [#3176]
- Do not consume mousebind scroll events under `<context name="Client">`.
  @elviosak [#3146] [#3168]
- Work around client-side rounding issues at right/bottom pixel. This fixes an
  issue with some clients (notably Qt ones) where cursor coordinates in the
  rightmost or bottom fixel are incorrectly rounded up putting them outside the
  surface bounds. The issue has been particularly noticeable with layer-shell
  clients like lxqt-panel. @jlindgren90 [#3157] [#2379] [#3099]
  Note: This also avoids a similar server-side rounding issue with some
  combinations of wlroots and libwayland versions. See:
  - https://gitlab.freedesktop.org/wayland/wayland/-/issues/555
  - https://gitlab.freedesktop.org/wlroots/wlroots/-/merge_requests/5159
- Don't remove newlines when parsing config, menu and XBM because doing so can
  cause parser error in some unusual situations like the one shown below.
  @tokyo4j [#3148]

```
<!--
 -
 - Some comments
 -
-->
```

### Changed

- Refactor window switcher configuration to put attributes `show` and `style`
  under `<windowSwitcher><osd>` rather than directly under `<windowSwitcher>`.
  The old configuration syntax will remain supported for at least one release.
  @dntxi [#3201]
- Place OSDs at the center of output rather than usable area @tokyo4j [#3179]
- If XML documents (like rc.xml and menu.xml) have an XML declaration (typically
  `<?xml version="1.0"?>`), this XML declaration must be the first thing in the
  document. In previous versions, line breaks (`\n`) were allowed before due to
  the way the files were parsed, but this is approach caused other issues like
  [#3145] and is contrary to XML syntax. [#3148] [#3153]
- With the window-switcher custom field state specifiers 's' and 'S', change the
  display order from M|m|F to m|s|M|F; and increase the size from three
  characters wide to four. @domo141 [#2895]
- Put labnag in overlay layer by default so that the dialog is still visible
  when a window is using fullscreen mode. @johanmalm [#3158]
- Call labnag with on-demand keyboard interactivity by default @tokyo4j [#3120]
- Temporarily unshade windows when switching windows. Restore old behaviour with
  `<windowSwitcher unshade="no"/>` @Amodio @Consolatis [#3124]
- In the classic style window-switcher, the default color of the selected window
  item has been changed to inherit the border color but with 15% opacity
  @tokyo4j [#3118]

## 0.9.2 - 2025-10-10

[0.9.2-commits]

### Added

- Allow `SnapToEdge` and `ToggleSnapToEdge` to combine two cardinal directions
  with the config option `combine="yes|no"`. [#3081] @tokyo4j
- Support `Border` context for mousebinds as an alias for `Top`...`BRCorner` to
  make configuration easier. @tokyo4j [#3047]
- Add window-switcher mode with thumbnails. This can be enabled with:
  `<windowSwitcher style="thumbnail">`. @tokyo4j [#2981]
- Add `toggle` option to `GoToDesktop` action. This has the effect of going back
  to the last desktop if already on the target. @RainerKuemmerle [#3024]
- Add `<theme maximizedDecoration="titlebar|none"/>` to allow hiding titlebar
  when window is maximized. @CosmicFusion @tokyo4j [#3015]
- Use client-send-to-menu as 'Workspace' submenu in built-in client-menu
  @johanmalm [#2995]
- Allow overwriting submenu icon to increase flexibility and enhance Openbox
  compatibility. @tokyo4j [#2998]
- Allow client-{list-combined,send-to}-menu as submenu of static menu @tokyo4j
  [#2994]
- Add `labnag` (a dialog client with message and buttons) and associated
  `<prompt>` option in 'If' actions.  @johanmalm @Consolatis @tokyo4j [#2699]
- Support config option `<core><promptCommand>` @johanmalm [#3097]
- Allow snapping to corner edges during interactive move with associated config
  options `<snapping><cornerRange>`. @tokyo4j [#2885]
- Support new values "up-left", "up-right", "down-left" and "down-right" with
  `<action name="(Toggle)SnapToEdge" direction="[value]">` and
  `<query tiled="[value]">`. @tokyo4j [#2885]
- XML parsing improvements as listed below. @tokyo4j [#2667] [#2967] [#2971]
  - Support nested `If` and `ForEach` actions
  - Parse CDATA as text all nodes
  - Remove ordering constraint of attributes in `<keybind>`, `<mousebind>` and
    `<windowRule>`
  - `If` actions now works for menus
  - For menus, the `name` argument no longer has to be the first argument of
    `<action>`; and the `label` argument no longer has to be the first argument
    of `<item>`
- Toggle mousebinds with the `ToggleKeybinds` action @tokyo4j [#2942]
- Add support for direction value 'any' with tiled queries. This allows users
  to query for any snap directions without using multiple query statements
  @lynxy [#2883]

### Fixed

- On detecting broken icon theme, fall back on 'hicolor' @Consolatis [#3126]
- Restore initially-maximized window position after unplug/plug @tokyo4j [#3042]
- Fix large client-side icon not being loaded when the rendered icon size is
  larger than icon sizes from the client. @tokyo4j [#3033]
- Improve debug logging for configuring input devices @jlindgren90 [#3028]
- Fix false positives when matching desktop entries @datMaffin [#3004]
- Prevent accidental downcasting of scale in scaled-icon-buffer to avoid blurry
  icons on non-integer scales and a cairo assert when using a output scale < 1.
  @Consolatis #2984
- Fix xdg-shell windows moving between outputs due to configure timeout
  @jlindgren90 [#2976]
- Fix segfault with toplevel `<separator>` in `menu.xml` @tokyo4j [#2970]
- Prevent hi-res mice triggering scroll actions too often @tokyo4j [#2933]

### Changed

- Change default keybind `W-<arrow>` to combine cardinal directions to support
  resizing of windows to fill a quarter of an output. This only affects users
  who do not use an `rc.xml` (thereby using default keybinds) or use the
  `<keyboard><default/>` option. Previous behavior can be restored by setting
  `combine="no"` as shown below. [#3081] @tokyo4j

```
<keybind key="W-Left">
  <action name="SnapToEdge" direction="left" combine="no" />
</keybind>
<keybind key="W-Right">
  <action name="SnapToEdge" direction="right" combine="no" />
</keybind>
<keybind key="W-Up">
  <action name="SnapToEdge" direction="up" combine="no" />
</keybind>
<keybind key="W-Down">
  <action name="SnapToEdge" direction="down" combine="no" />
</keybind>
```

- `Focus` and `Raise` on window border press because it is probably what most
  people expect and it makes the behavior consistent with that of Openbox.
  @johanmalm [#3039] [#3049]
- On interactive resize, only un-maximize the axis/axes that are being resized.
  @jlindgren90 [#3043]
- Change theme setting `osd.window-switcher.*` to
  `osd.window-switcher.style-classic.*`. Backward compatibility is preserved.
  @tokyo4j [#2981]
- In client-list menu, add brackets around the titles of any minimised windows
  @davidphilipbarr [#3002]
- Respect client-initiated window resize of non-maximized axis, for example
  remember the width of vertically-maximized window resizing itself
  horizontally. @jlindgren90 [#3020]
- Remember position of window along non-maximized axis during interactive move.
  @jlindgren90 [#3020]
- Restore default libinput device values on reconfigure with empty value, rather
  than leaving the old configuration. This makes rc.xml more declarative.
  @tokyo4j [#3011]
- Change `If` action when used without a focused window to execute the `<else>`
  branch (previously it was just ignored). The reason for this is to make things
  more consistent with `<prompt>`. It is not anticipated that this will affect
  anyone's workflow but is mentioned here for completeness.
- Make `autoEnableOutputs=no` apply only to drm outputs @jlindgren90 [#2972]
- Take into account `<core><gap>` for edge and region overlays @tokyo4j [#2965]

## 0.9.1 - 2025-08-02

[0.9.1-commits]

This is an earlier-than-usual release containinig bug fixes only. It has been
done on a separate branch to avoid the inclusion of refactoring and new
features.

```
               0.9.1  <--- bug-fixes only
                /
               /
0.8.4--------0.9.0--------  <-- master
```

### Fixed

- Prevent interaction with un-initialized xdg-shell windows after unmap to fix a
  bug exposed by `wlroots-0.19.0` resulting in a compositor crash in certain
  (unusual) circumstances [#2948] [#2937] [#2944] @Consolatis
- Fix double-free in `img_svg_render()` failure path [#2910] @jlindgren90
- Fix swapped width/height in XWayland client `_NET_WM_ICON` stride calculation
  [#2909] @jlindgren90

## 0.9.0 - 2025-07-11

[0.9.0-commits]

The main focus has been to port labwc to wlroots 0.19 [#2388] and fix associated
issues. Special thanks to @Consolatis @jlindgren90 for this.

### Added

- Add client `lab-sensible-terminal` and add a `Terminal` entry to the default
  root-menu @johanmalm [#2877]
- Enhance `-v|--version` option by adding feature flags like `+xwayland -rsvg`.
  @Consolatis [#2873]
- Send drm leases to XWayland clients. This requires XWayland >= 21.1.9.
  @Consolatis [#553] [#2873]
- Add `<windowRule iconPriority="client|server">`. @Consolatis @tokyo4j [#2839]
- Support theme colors defined by X11-color-names and '#rgb' syntax @jlindgren90
  [#2686]
- Support basic vertical titlebar gradients and the additional theme options
  listed below. @jlindgren90 [#2686]

```
window.*.title.bg: Solid | Gradient ( Vertical | SplitVertical )
window.*.title.bg.colorTo:
window.*.title.bg.color.splitTo:
window.*.title.bg.colorTo.splitTo:
```

- Support the XWayland `_NET_WM_ICON` property. Use the new `iconPriority`
  window rule to enable this. @Consolatis @tokyo4j [#2840]
- Add config option `<core><primarySelection>`. This enables autoscroll
  (middle-click to scroll up/down) in Chromium and electron based clients
  without inadvertantly pasting the primary clipboard. @johanmalm [#2832]
- Bump `xdg_shell` version from 3 to 6 @tokyo4j [#2814]
- Bump `wl_compositor` version from 5 to 6 @tokyo4j [#2812]
- Support tablet tool mouse buttons @jp7677 [#2778]
- Add libinput config options:
  - `<threeFingerDrag>` @m4rch3n1ng [#2795]
  - `<dragLock>sticky</dragLock>` @tokyo4j [#2803]
  - `<scrollMethod>none|twofinger|edge</scrollMethod>` @Consolatis [#2767]
- Add `{left,right}-occupied` options to `GoToDesktop` @DreamMaoMao [#2790]
- Add config option `<theme><dropShadowsOnTiled>` @diredocks [#2789]
- Add missing tracking of configure serials for xdg-shell surface to fix issue
  with mpv @jlindgren90 [#2774] [#2788]
- Add support for the following Wayland protocols:
  - `ext-data-control` @Consolatis [#2829]
  - `alpha-modifier` @Consolatis [#2829]
  - `xdg-toplevel-icon protocol`. Use the new `iconPriority` window rule to
     enable this. @tokyo4j [#2755]
  - `drm-syncobj` protocol @zeusgoose [#2737]
  - `ext-image-copy-capture` protocol @any1 [#2740]
- Support both axis for XWayland client side maximize requests.
  @Consolatis [#2728]
- Add scroll emulation for cursor motion and associated actions @jp7677
  [#2678]:
   - `EnableScrollWheelEmulation`
   - `DisableScrollWheelEmulation`
   - `ToggleScrollWheelEmulation`

### Fixed

- Fix flicking with negative screen/window resistance @ahesford [#2886]
- Fix layer-shell UAF bug on TTY change @johanmalm @Consolatis [#2874]
- Allow dragged windows to be moved to other workspaces. @Sumandora [#2868]
- Destroy xdg-shell popups when their parent is destroyed to fix potential
  compositor crash. @Consolatis [#2846]
- Clear SSD hover effects after touch-up @jp7677 [#2837]
- Close compositor menus on first touch up/down event to prevent menus from
  staying open during touch interactions in native touch mode. @jp7677 [#2827]
- Omit pointer cursor shape for tablet tools to prevent a resize cursor for
  out-of-surface scrolling with a tablet tool in recent GTK4 (which uses the
  cursor shape protocol). @jp7677 [#2808]
- For XWayland, give focus to a modal dialog rather than its parent.
  @jlindgren90 [#2722]
- Do not send configure events in unmap handler to fix issues with `wshowkeys`
  and `kitten` @tokyo4j @johanmalm [#1153] [#1154] [#2867]
- Window switcher fixes: @tokyo4j [#2770]
  - Always show title with `<field content="title">`. Before this patch, titles
    were not shown if identical to identifiers.
  - Always show output name with `<field content="output">`. Before this patch,
    output names were not shown if there was only one output.
- Send fractional scale to layer-shell surfaces before map. @Consolatis [#2768]
- Only configure initialized layer-shell surfaces to fix bug with
  `kitten quick-access-terminal` @alex-huff [#2736] [#2745] 
- Improve focus semantics for XWayland windows using the Globally Active input
  model to fix issues with Zoom, WeChat and CLion @jlindgren90 [#1142]
  [#2811] [#2819]
- Provide better support for XWayland client keyboard focus grabs by using the
  new `grab_focus` signal. @jlindgren90 [#1142]
- Guard against negative sizes in window-switching and menu graphical artefacts.
  @tokyo4j [#2727]
- Do not broadcast keyboard modifiers from virtual keyboards to fix issue with
  per-window layout settings. @orfeasxyz [#2723] [#2724]
- Gracefully exit when no fonts are installed @tokyo4j [#2713]
- config: validate total osd field width to ensure it does not exceed 100%.
  @tokyo4j [#2710]

### Changed

- Replace alacritty in default keybind with `lab-sensible-terminal` @tokyo4j
  [#2891]
- Use the `Super` modifier instead of `Alt` for the default mousebinds `A-Left`
  and `A-Right` (for move and resize) to avoid interfering with some clients
  like CAD programs and games @johanmalm [#2831]
- Deprecate the default keybinds listed below. @johanmalm [#2831]
  - `A-F3` for bemenu-run because it is too close to A-F4 and it is better to be
    agnostic on choice of launcher.
  - `A-<arrow>` for `MoveToEdge` because `Alt-` keybinds should be for clients
    to use and this one results in frequent user complaints because it prevents
    some common usage patterns like alt-left/right in web browers.
- Change default titlebar menu button from a dot to an arrow @johanmalm [#2844]
- When `dragLock` is set to `yes`, the drag no longer expires after a short
  delay (known as `Sticky` mode) as recommended by libinput [#2803]. The timeout
  based behavior can be restored via the snippet below.

```xml
<libinput>
  <device>
    <dragLock>timeout</dragLock>
  </device>
</libinput>
```

## 0.8.4 - 2025-05-02

[0.8.4-commits]

This release predominantly consists of bug-fixes, code simplification and
usability improvements. Amongst the new features the most noteworthy is the
addition of icons support in the window-switcher and client-list-combined-menu.

A big thank you to @tokyo4j for leading the way on a lot of work in this
release.

### Added

- Support all pango font weight options (normal, thin, ultralight, light,
  semilight, book, medium, semibold, bold, ultrabold, heavy, ultraheavy) via
  config option `<theme><font><weight>` @spl237 [#2692] [#2693]
- Add theme option `osd.workspace-switcher.boxes.border.width` @czkz [#2657]
- Add theme option `osd.window-switcher.item.icon.size` @tokyo4j [#2651]
- Localize desktop-entry application names used by the window switcher via
  `desktop_entry_name` or the `%n` specifier @tokyo4j [#2653]
- Add `HideCursor` action @jp7677 [#2633]
- Support application icons in window-switcher using `<field content="icon"/>`
  and use this by default. @tokyo4j [#2621]
- Support application icons in client-list-combined-menu @tokyo4j [#2617]
- Support the use of the keypad-enter key when using menu. @zeusgoose [#2610]
- Show fallback icon in SSD titlebar when no `app_id` is set via
  `<theme><fallbackAppIcon>` @tokyo4j [#2599]

### Fixed

- Enable overriding of `<touch>` configs to fix `--merge-config` bug @spl237
  [#2700]
- Handle initially minimized windows (for example VSCode) to fix a focus and
  stacking bug @jlindgren90 [#2688] [#2627]
- Minor window-switcher fix for box size and alignment @czkz [#2657]
- Overwrite (not amend) configuration entries for `<windowSwitcher><fields>`
  and `<theme><titlebar><layout>` in support of using --merge-config @tokyo4j
  [#2669]
- Consider `item.padding.y` when centering workspace name in window-
  switcher @tokyo4j [#2651]
- Notify XWayland of correct window stacking order to fix issue with mouse
  scroll events and always-on-top windows. @tokyo4j [#2638]
- Scale and transform magnifier in accordance with output settings @tokyo4j
  [#2645]
- Allow only `Previous/NextWindow` action while window switching to prevent
  undefined behaviour like using `SendToDesktop` while window switching.
  @tokyo4j [#2613]
- Harden window stacking order while window switching @tokyo4j [#2613]
- Do not update cursor while window switching @tokyo4j [#2613]
- Honor no content `<desktops><prefix>` node because users need a way to
  override the default "Workspace". @johanmalm [#2601] [#2613]

### Changed

- Change default window-switcher layout to show icons and desktop-entry
  application name @tokyo4j [#2648]
- If `<focus><followMouse>` is set to yes, the focus is now updated only when
  the cursor enters a window content, not when the cursor moves within the
  whole window including the titlebar. This makes the behaviour consistent with
  that of kwin, xfwm4 and openbox @tokyo4j [#2652]
- Show magnifier only on one output to simplify handling of different scales
  and transforms. @tokyo4j [#2645]
- Center labwc.svg logo vertically @jlindgren90 [#2619]
- Increase default `<snapping><range>` to 10 to make it easier to snap windows
  on the edge between two monitors. @johanmalm [#2602] [#2608]

## 0.8.3 - 2025-02-21

[0.8.3-commits]

The eye-catching new features of this release are undoubtedly:
1. Support for the `ext-workspace` protocol with big thanks to @Consolatis
2. Menu enhancements including icons and dynamic root-menus. Credits here go to
   @Consolatis, @tokyo4j and @johanmalm for improvements to both backend buffer
   managements and front end menu mechanics.

However - on the whole - the main effort of this release has gone into
stability, usability and performance fixes, and it really feels like we have
matured nicely against the wlroots 0.18 series.

Notes to package maintainers:
- This version introduces the ext-workspace protocol which breaks xfce4-panel
  4.20.0. There is a fix in subsequent releases so make sure xfce4-panel is
  shipped at >= 4.20.1.

### Added

- Add config options `<resize><cornerRange>` and `<resize><minimumArea>` [#2529]
- Menu icons support [#2509]

```xml
<menu>
  <showIcons>yes</showIcons>
</menu>
```

- Support toplevel pipemenus [#2238] [#2239]

```xml
<?xml version="1.0"?>
<openbox_menu>
  <menu id="root-menu" label="" execute="labwc-menu-generator -p -I" />
</openbox_menu>
```

- Add `<theme><fallbackAppIcon>` to specify the icon name to be used when
  lookups for an application icon have failed [#2518]
- Use 'labwc' directory for themes as well as 'openbox-3' [#2488]
- Add default Alt-Shift-Tab keybind for `PreviousWindow` [#2477]
- Add config option `<core><autoEnableOutputs>` to allow users to avoid
  automatically enabling outputs at startup and when new outputs are connected.
  With autoEnableOutputs disabled, tools such as kanshi can be used to give
  finer-grained control of which outputs are enabled, which may be useful to
  avoid re-enabling outputs that disconnect and reconnect during powersave
  [#2458]
- Add WarpCursor action. Written-by: @orfeasxyz [#2118]
- Support ext-workspace protocol [#2365]

### Fixed

- Prevent black flash (caused by unnecessary output commit without buffer) when
  repeatedly calling `wlopm --on` [#2580]
- Set custom output mode on `wlopm --on` to work around a wlroots issue:
  https://gitlab.freedesktop.org/wlroots/wlroots/-/issues/3946 [#2578]
- Handle layer-shell unmap without any outputs left [#2577]
- Fix some odd, inconsistent behaviour with resize edges [#2529]
- Cleanup overlay timer on exit [#2574]
- Fix unexpected behavior when a menu is opened from another menu [#2537]
- Taking into account SSD margin in `MoveTo` action [#2469] [#2563]
- Fix UAF caused by trying to update xcursor in an output destroy-handler
  [#2539] [#2560]
- Send wlr-foreign-toplevel `output_enter` on initialization to fix a bug which
  causes missing taskbar items in Waybar when configured to show windows per
  output. [#2550]
- Use subsurface as reference for out-of-surface cursor movement [#2542] [#2547]
- Dynamically look up window icons in server-side-deco titlebar for output
  scales [#2518]
- Do not leak bound scroll events from touchpad to clients in some conditions
  [#2516]
- Ignore duplicated buttons in `<titlebar><layout>` rather than ignoring all of
  it [#2524]
- Set repeat information for virtual keyboards [#2513]
- Honour modifier state of virtual keyboards when processing mousebinds. Helps
  some use-cases under wayvnc [#2511]
- Accept uppercase icon endings
- IME: fix stuck Ctrl when pressed Ctrl+F in Firefox with Fcitx5 [#2498]
- Fix invisible cursor on application after reconfigure [#2499]
- Fix abort() on non-ARGB32 PNG file [#2495]
- Ignore `wlr_output_state.mode/custom_mode` except for client request [#2486]
- Cancel keyboard keybind-repeat on reconfigure [#2473]
- Send initial wlr-foreign-toplevel pre-map state [#2460]
- Fix NULL `string_prop` crash when `app_id` is NULL [#2453]
- Fix scaling for rendering large image in non-square rectangle [#2451]
- Fix cursor focus when menu is closed by clicking its border [#2443]
- Update pointer focus on xdg-popup/layer popup destruction to fix bug where
  closing a popup did not move the pointer focus to the main toplevel until the
  cursor was moved. [#2443]
- Improve algorithm for menu placement with xdg-positioner [#2408]
- Do not forward IME key-release without correspinding key-press to avoid stuck
  keys [#2437]

### Changed

- Make window switcher more Openbox-like in terms of key processing. The shift
  key no longer inverts the direction of window switching, so to keep the
  original behavior, alt-shift-tab has been added as a default keybind to cycle
  to the previous window [#2477]
- Do not set numlock by default, only set on|off if user specifically requests
  it in the config file [#2483]
- Demote libsfdo error-logging to `WLR_INFO` to avoid logging issues with
  .desktop files as errors [#2456]
- Always send modifier release events. This means that when a keybind is
  triggered while focusing on an application, the key-release event of the
  modifier key is now sent to the application. This fixes the problem of stuck
  modifier keys in some applications including Blender and wlfreerdp. Note that
  this change makes existing keybinds with Alt key show Firefox's menu bar. For
  those who don't want to make those keybinds interfere with Firefox's menu bar,
  we recommend replacing Alt key (e.g. "A-s") with Win key (e.g. "W-s") in the
  keybinds. [#2455]
- Removed `wantAbsorbedModifierReleaseEvents` window rule as it's no longer
  needed [#2455]
- Clear the keyboard/pointer focus while a window is dragged, the window
  switcher is activated or a menu is opened. As a result, with followMouse="yes"
  and followMouseRequiresMovement="no" in rc.xml, keyboard-focus semantics
  have subtly changed when using the window-switcher. [#2455]

## 0.8.2 - 2024-12-13

[0.8.2-commits]

This is a shorter release cycle compared with the usual 10-week one because it
contains a significant number of stability and cleanliness fixes which warrant
packaging.

We do not normally describe behind-the-scenes work in this log, but will mention
two here as an exception:

1. A clean run with gcc/clang memory leak check has been achieved. Thanks to
   @tokyo4j for fantastic work with this [#2331]
2. A buffer-sharing mechanism has been merged to improve both processor and
   memory usage in the long term. Credits to @tokyo4j and @Consolatis for this
   one. [#2363]

Notes to package maintainers:

- The wlroots dependency has been increased to `0.18.1` to avoid a crash when
  using ext-foreign-toplevel-list protocol.
- It is also advisable to use `0.18.2` as soon as possible to fix a crash
  triggered when the xwayland server closes during a drag-and-drop with an
  XWayland client.

### Added

- Add support for xdg-foreign-v1 and xdg-foreign-v2 protocols [#2400]
- Add window rule to send release-events of modifiers which are part of
  keybinds. This supports clients (like blender) that want to see modifier
  release events even when they are part of a keybinds. [#2377]

```xml
<windowRules>
  <windowRule identifier="blender" wantAbsorbedModifierReleaseEvents="yes"/>
</windowRules>
```

- Support menu borders [#2376]

```
menu.border.width: 1
menu.border.color: #aaaaaa
```

- Add conversion specifier `%n` to the window-switcher `<field>` config option
  to show desktop entry name in the window-switcher. Written-by: @jp7677 [#2360]

```xml
<windowSwitcher>
  <fields>
    <field content="custom" format="%n" width="25%"/>
    <field content="title" width="75%"/>
  </fields>
</windowSwitcher>
```

- Add hold gestures. @jp7677 [#2326]
- Support ext-foreign-toplevel-list protocol [#2072]
- Support additional window rule conditions including `shaded`, `maximized`,
  `iconified`, `focused`, `omnipresent`, `desktop`, `tiled` and `tiled_region`.
  This also works for `If` and `ForEach` queries. @orfeasxyz @ahesford [#2245]
- Add mouse emulation for touch devices. @spl237 [#2277]
- Improve handling of touch events. @jp7677 [#2273]
  This includes:
    - Hide the cursor on touch input and keep the cursur invisible until
      pointer or tablet input
    - Close xdg-popups on touch down
    - Notify idle-manager on touch down/up
    - Clear pointer focus on touch input to avoid pointer focus interfering
      with touch input, like showing hover effect on unexpected locations
    - Move touch only with one touch point - in other words do not move the
      cursor when more than one finger is down
    - Warp cursor to touch coordinates for consistent behaviour with
      non-touch capable surfaces including the desktop
- Set environment variable `LABWC_VER` with current compositor version.
  @01micko [#2257]
- Broadcast keyboard modifiers to all clients rather than just the one with
  keyboard focus. [#2274]
  This enables:
    - Clients such as panels to display the current keyboard layout without
      introducing new wayland protocols or other IPC.
    - Unfocused xdg-shell clients to understand button press with keyboard
      modifiers for example Ctrl+click.

### Fixed

- Fix crash caused by `rc.xml` `<touch>` options being specified as
  elements rather than attributes. [#2412]
- Fix `ShowMenu` action position with x/y arguments in multi-monitor setup.
  [#2409]
- Block privileged protocols for sandboxed clients [#2398]
- Fix incorrect focus behaviour when switching between workspaces with
  omnipresent windows open [#2335]
- Fall back to loading icon based on app-id when `Icon` defined in .desktop file
  can not be loaded [#2361]
- Fix regression introduced with `0.8.1` to allow negative values for theme
  option `menu.overlap` [#2356]
- Ensure output is usable before setting adaptive sync [#2337] [#2338]
- Fix `menu.title.text.justify: right` not working [#2336]
- Keep focus on omnipresent windows when switching workspaces [#2329]
- Skip painting output when session is not active. @enometh @Madhu [#2249]
- Ignore variable assignments > 1 KiB in environment files to guard against
  recursive constructs like FOO=$FOO:bar which would grow on each reconfigure.
  [#2325]
- Improve support for non-compliant .desktop files by matching partial strings
  to handle for example app-id="gimp-2.10" with file "gimp.desktop". @spl237
  [#2266]
- Correctly center menu opened with `<position {x,y}="center">` @tokyo4j [#2319]
- Allow pointer speed of -1.0. @spl237 [#2321]
- Fix off-by-one bug in `buf_add_char()` [#2313]
- Fix menu separator-line padding regression introduced in `0.8.1`. @domo141
  [#2291]
- Avoid permanent disabling of tearing due to rejected commits caused by the
  cursor plane not allowing async page flips which causes tearing page flips
  to be rejected if the cursor is moved. @RicArch97 [#2295]
- Use `MenuHeader` font height in separators with labels. @domo141 [#2276]

### Changed

- Set xwaylandPersistence default value to `yes` when compiled with wlroots
  <0.18.2. This prevents a bug which has the potential to crash the compositor
  when performing a drag-and-drop action at the same time as the XWayland server
  is shutting down. [#2371] [#2414] [#2420]
- Set default window placement policy to `cascade` instead of `center` [#2345]

```xml
<placement>
  <policy>cascade</policy>
</placement>
```

- Set default values of theme option `window.*.border.color` to `#aaaaaa`. This
  makes the colors of window borders and titlebar different, but will let
  `menu.border.color` inherit `window.active.border.color` just like Openbox
  does, without making the menu borders around a selected menu item invisible.
  [#2376]
- Invert the y-offset of submenus applied by `menu.overlap.y` to (i) follow
  Openbox's behavior and (ii) behave as already described in our own
  documentation. [#2380]

## 0.8.1 - 2024-10-25

[0.8.1-commits]

The most noteworthy additions in this release are:

1. Titlebar window icons and layout configuration
2. Support for the cosmic-workspace protocol and the openbox inspired
   client-list-combined-menu for a better user experience with workspaces.

Notes to package maintainers:

- The SSD titlebar window icon support requires [libsfdo] to be added as a
  (build and run-time) dependency or statically linked. If this is not wanted,
  add `-Dicon=disabled` to the `meson setup` command in the build script for the
  next release.
- PRs [#1716] and [#2205] add labwc xdg-portal configuration, modify
  `labwc.desktop` and amend `XDG_CURRENT_DESKTOP` which should enable better
  out-of-the-box support for xdg-desktop-portal, but if you already ship a
  custom setup for this or have different requirements, please review this
  change.

[libsfdo]: https://gitlab.freedesktop.org/vyivel/libsfdo

### Added

- Support dmabuf feedback [#2234] [#1278]
- Add initial implementation of cosmic-workspace-unstable-v1 [#2030]
- Optionally support SSD titlebar window icons. When an icon file is not found
  or could not be loaded, the window menu icon is shown as before. The icon
  theme can be selected with `<theme><icon>` [#2128]
- Add actions `ToggleSnapToEdge` and `ToggleSnapToRegion`. These behave like
  `SnapToEdge` and `SnapToRegion`, except that they untile the window when
  already being tiled to the given region or direction.
  Written-by: @jp7677 and @tokyo4j [#2154]
- Add action `UnSnap`. This behaves like `ToggleSnapToEdge/Region` but
  unconditionally. Written-by: @jp7677 and @tokyo4j [#2154]
- Handle xdg-shell `show_window_menu` requests [#2167]
- Support the openbox style menus listed below. Written-by: @droc12345
  1. `client-list-combined-menu` shows windows across all workspaces. This can
     be used with a mouse/key bind using:
     `<action name="ShowMenu" menu="client-list-combined-menu"/>` [#2101]
  2. `client-send-to` shows all workspaces that the current window can be sent
     to. This can additional be used within a client menu using:
     `<menu id="client-send-to-menu" label="Send to Workspace..." />` [#2152]
- Add theme option for titlebar padding and button spacing [#2189]

```
window.button.height: 26
window.titlebar.padding.width: 0
window.titlebar.padding.height: 0
window.button.spacing: 0
```

- Set titlebar height based on the maximum height of any of the objects within
  it, rather than just taking the font height into account [#2152]
- Add theme option for setting button hover effect corner radius [#2127] [#2231]

```
window.button.hover.bg.corner-radius: 0
```

- Add position arguments for menus. Written-by: @droc12345 [#2102]

```xml
<action name="ShowMenu">
  <menu>root-menu</menu>
  <position>
    <x>0</x>
    <y>0</y>
  </position>
</action>
```

- Allow interactive window movement when horizontally or vertically maximized
  and add associated config option `<resistance><unMaximizeThreshold>` [#2052]
- Add optional Shade (shade.xbm) and AllDesktops (desk.xbm) buttons and theme
  options:

```
window.active.button.desk.unpressed.image.color
window.inactive.button.desk.unpressed.image.color
window.active.button.shade.unpressed.image.color
window.inactive.button.shade.unpressed.image.color
```

- Make action `FocusOutput` behave like `MoveToOutput` by adding direction and
  wrap arguments. Written-by: @orfeasxyz [#2100]
- Add config option for titlebar layout. Written-by: @xi [#2088] [#2150]

```xml
<titlebar>
  <layout>icon:iconify,max,close</layout>
  <showTitle>yes|no</showTitle>
</titlebar>
```

- Add `Oblique` option to `<theme><font><style>`. Written-by: @droc12345 [#2097]
- Support menu titles defined by `<separator label="">`.
- Add the theme option `menu.title.bg.color: #589bda`
- Add theme options `menu.title.text.color` and `menu.title.text.justify`.
  Written-by: @droc12345 [#2097]
- Add font place MenuHeader: `<font place="MenuHeader">`.
  Written-by: @droc12345 [#2097]
- Add actions `EnableTabletMouseEmulation` and `DisableTabletMouseEmulation`.
  Written-by: @jp7677 [#2091]
- Set 'labwc' as `app_id` and `title` for nested outputs [#2055]

### Fixed

- Fix rare NULL-dereference when using cursor constraints [#2250]
- Fix issue where tablet/touchscreen button events sometimes do not take effect
  on applications immediately [#2244]
- Fix button release events sometimes not being sent [#2226]
- Fix xdg-shell popups appearing on wrong output with some Qt themes. [#2224]
- Take into account xdg-shell minimum window size for resizing. This is
  relevant when using `<resize drawContents="no">` [#2221]
- Fix rounded hover effect on titlebar buttons when the window is tiled or
  maximized [#2207]
- Fix button scaling issue [#2225]
- Add portals.conf file, amend `labwc.desktop` and modify `XDG_CURRENT_DESKTOP`
  for better out-of-the-box xdg-desktop-portal support. This helps with for
  example screensharing. Written-by: @rcalixte @jp7677 [#1503] [#1716]
- Disable the Inhibit D-BUS interface in xdg-portals configuration to fix an
  issue with some clients (like Firefox) ignoring the idle-inhibit protocol.
  Written-by: @jp7677 [#2205]
- Prevent `Drag` mousebinds from running without button press [#2196]
- Handle slow un-maximize with empty natural geometry better [#2191]
- Fix de-synced SSD when shrinking Thunderbird xdg-shell window [#2190]
- Fix xdg-shell out-of-sync configure state when clients time out
  Written-by: @cillian64 [#2174]
- Fix small flicker when client initially submits a window size smaller than the
  minimum value [#2166]
- Allow server-side decoration to be smaller than minimal size by hiding
  buttons [#2116]
- Fix incorrect cursor shape on titlebar corner without buttons [#2105]
- Fix delayed pipe menu response on item destroy [#2094]
- Destroy xdg-shell foreign toplevel handle on unmap [#2075]
- Sync XWayland foreign-toplevel and associated outputs on re-map [#2075]

### Changed

- Theme options `padding.height` and `titlebar.height` have been removed to
  minimize breaking changes with the visual appearance of the titlebar when
  using openbox themes. As a result, and depending on your configuration,
  the titlebar height may change by a small number of pixels [#2189]
- Move input config `<scrollFactor>` to `<libinput>` section to allow
  per-device configuration of scroll factor (e.g. setting different scroll
  factors for mice and touchpads). [#2057]

## 0.8.0 - 2024-08-16

[0.8.0-commits]

The main focus in this release has been to port labwc to wlroots 0.18 and to
grind out associated regressions. Nonetheless, it contains a few non-related
additions and fixes as described below.

There are a couple of regression warnings when using wlroots 0.18:

1. There appears to be an issue with increased commit failures, particularly
   with intel drivers. If this turns out to be an issue for anyone please try
   running with `WLR_DRM_NO_ATOMIC=1` or run the labwc v0.7 branch or its latest
   release until this is resolved.
2. Fullscreen VRR is broken but should be fixed once wlroots 0.18.1 is released.
   Again, if that is a problem we advise to stay with the v0.7 branch in the
   short term until fixed. [#2079]

A v0.7 branch has been created for bug fixes beyond `0.7.3` (built with wlroots
`0.17`).

A big thank you goes to @Consolatis for carefully crafting the commits to port
across to wlroots 0.18.0. Many thanks also to the other core devs @ahesford,
@jlindgren90, @johanmalm and @tokyo4j for reviewing, merging as well as
contributing many patches with fixes and new features. And in this release we
have some great contributions from @jp7677, @kode54, @xi and @heroin-moose which
have been attributed with a 'Written-by' against each relevant log entry.

### Added

- Add options `fullscreen` and `fullscreenForced` for `<core><allowTearing>`
  Written-by: @jp7677 & @Consolatis [#1941]
- Optionally allow keybindings when session is locked, which for example can be
  useful for volume settings. Written-by: @xi [#2041]

```xml
<keyboard><keybind key="" allowWhenLocked="">
```

- Add resistance when dragging tiled/maximized windows with config option
  `<resistance><unSnapThreshold>`. [#2009] [#2056]
- Implement support for renderer loss recovery. Written-by: @kode54 [#1997]
- Support xinitrc scripts to configure XWayland server on launch. [#1963]
- Add theme option `window.button.width` to set window button size.
  Written-by: @heroin-moose [#1965]
- Add `cascade` placement policy [#1890]

```xml
<placement>
  <policy>cascade</policy>
  <cascadeOffset x="40" y="30"/>
</placement>
```

- Support relative tablet motion. Written-by: @jp7677 [#1962]

```xml
<tabletTool motion="absolute|relative" relativeMotionSensitivity="1.0"/>
```

### Fixed

- Make tablet rotation follow output rotation. Written-by: @jp7677. [#2060]
- Fix error when launching windowed Chromium. [#2069]
- Fix empty `XKB_DEFAULT_LAYOUT` bug. [#2061]
- Take into account CSD borders when unconstraining XDG popups. [#2016]
- Choose xdg-popup output depending on xdg-positioner [#2016]
- Fix wlroots-0.18 regression causing flicker with some layer-shell clients like
  fuzzel on launch. [#2021]
- Fix incorrect condition in server-side-deco logic [#2020]
- Fix flicker of snapped windows in nested session. [#2010]
- Fix tearing with atomic mode setting. Written-by: @kode54 [#1996]
- Handle initially maximized and fullscreen xdg-shell windows better.
  [#1956] and [#2007]
- Set initial geometry of maximized and fullscreen XWayland windows in the
  `map_request` handler to avoid visual glitches with some apps. [#1529]
- Disable pango glyph position rounding to avoid text geometry jump around when
  changing scale.

### Changed

- Make windows stay fullscreen when associated output is disconnected. [#2040]

## 0.7.4 - 2024-06-19

[0.7.4-commits]

### Fixed

- Make SSD borders respect snapped state on Reconfigure. [#2003]
- Fix magnifier by disabling direct scanout when active. [#1989]
- Fix crash triggered by pipemenu without parent `<menu>` element. [#1988]

## 0.7.3 - 2024-06-12

[0.7.3-commits]

Following a couple of big releases, this one feels like more steady with lots of
focus on bug fixes and stability. In terms of new features the most noteworthy
ones include improved tablet support (by @jp7677), `Super_L` on-release keybinds
(by @spl237 from the Raspberry Pi teams) and the screen magnifier which was a
joint effort by @spl237 and @Consolatis.

### Added

- Add config option `<core><xwaylandPersistence>` to support keeping XWayland
  alive even when no clients are connected. [#1961]
- Support xdg-shell protocol v3 with popup repositioning. [#1950]
  Also see https://gitlab.freedesktop.org/wlroots/wlroots/-/merge_requests/3514
  which adds support on the wlroots side.
- Add action `ToggleTabletMouseEmulation`. Written-by: jp7677 [#1915]
- Implement `<resize><drawContents>`. With thanks to @tokyo4j [#1863]
- Add `onRelease` option to `<keybind>` in support of binding `Super_L` to a
  menu. Written-by: @spl237 [#1888]
- Add initial support for `security-context-v1` (user configurable blocklists
  are still missing). Written-by: @nesteroff [#1817]
- Add support for `tablet-v2-manager`. Written-by: @jp7677 [#1678] [#1882]
- Add action `UnMaximize`. [#1831]
- Support multiple IME popups. [#1823]
- Add `All` context for mouse bindings which need to be handled irrespective of
  the mouse pointer location. This enables Super+mouse-scroll to change
  magnification. Written-by: @spl237 [#1768]
- Add `SetDecorations` action. Written-by: @xi [#1733]
- Add `policy` option to `AutoPlace` action. [#1784]
- Add window type filter to If-actions. Written-by: @xi [#1731]
- Add screen magnifier which can be controlled with the `ZoomIn`, `ZoomOut` and
  `ToggleMagnify` actions. Written-by: @spl237 [#1774]

### Fixed

- When looking for menu.xml, go through all paths rather than just giving up
  if not found in the first path searched. This makes it consistent with how
  other config/theme files are handled. [#1971]
- Fix memory leaks in theme.c and menu.c. [#1971]
- Fix session-lock bugs related to keyboard focus. [#1952]
  - Clear focused surface on lock
  - Restore focused view on unlock
- Fix memory leak in ssd/ssd-shadow.c [#1954]
- Respect `menu.overlap.x` when using pipemenus. [#1940]
- Do not try to restore windows to very small width/height on unmaximize.
  This fixes a bug with Thonny (Python IDE made with Tk). [#1938]
- Conditially set squared server-side decoration (SSD) corners when a view is
  tiled. Written-by: @jp7677 [#1926]
- Remember initial direction when starting window-cycling with `PreviousView`.
  Also make the toggling of direction when shift is pressed relative to the
  initial direction. For example if W-j is bound to PreviousWindow, subsequent
  key presses will continue to cycle backwards unless shift is also pressed.
  Written-by: @droc12345 [#1919]
- Show dnd icon above layer-shell surfaces. [#1936]
- Initialize locale after reading environment files so that client-menu items
  and workspace names follow the env var `LANG` should that be set in
  `~/.config/labwc/environment` (which is not recommended, but we prefer to
  handle it properly if it is). [#1927]
- Fix crash on `menu.xml` containing `<item>` without a parent `<menu>`.
  [#1907]
- Reset XWayland cursor image on cursor theme reload to avoid trying to read
  destroyed pixel data. [#1895]
- Prevent child views from opening outside of usable area. [#1878]
- Fix IME popups issues (flicker when popup surface is initially mapped
  and incorrectly showing multiple popups). [#1872]
- Rate-limit cursor-driven resize events based on monitor's refresh rate. This
  fixes the lag when resizing windows of some apps on XWayland, for example
  Chromium and Steam. [#1861]
- Session-lock: fix flashing & update cursor shape. [#1858]
- Remove tearing-controller listeners on destroy. [#1853]
- Handle invalid `ForEach` and `If` action cofigs. [#1838]
- Delay startup of applications until event loop is ready. This avoids race
  conditions when using autostart scripts that trigger a labwc SIGHUP. [#1588]
- With `SendToDesktop` action follow=no option, ensure the topmost window is
  focused. [#1800]
- Prevent XWayland from using incorrect keymap. [#1816]
- Allow keybinds containing the hyphen key to be defined with `-`.
  Written-by: @toast [#1811]
- Show/hide `top` layer more smartly. Before this commit, `top` layers were
  hidden whenever there was a fullscreen window in the corresponding output.
  With this commit, `top` layers are hidden only when there is a fullscreen
  window without other windows above it in the corresponding output.

### Changed

- Remove subprojects/seatd.wrap as no longer needed
- Action `MoveToCursor` is deprecated in favour of:
  `<action name="AutoPlace" policy="cursor"/>`.

## 0.7.2 - 2024-05-10

[0.7.2-commits]

This release shaped up to be the second in a row that is larger than usual in
terms of both fixes and new features. Significant additions include
input-methods, pipemenus, snap-to-edge overlays and optionally drop-shadows.

As usual, most of the commits are by the core devs: @ahesford, @Consolatis,
@jlindgren90, @johanmalm and @tokyo4j, but we also have many great
contributions from others as noted in the log.

### Added

- Add `<menu><ignoreButtonReleasePeriod>` to prevent clicks with small movements
  from inadvertantly closing a menu or selecting a menu item. This is the
  equivalent of `<menu><hideDelay>` on Openbox. [#1760]
- Support drop-shadows (disabled by default) for windows using server-side
  decorations. Written-by: @cillian64

```xml
<theme>
  <dropShadows>yes|no</dropShadows>
</theme>
```

```
window.active.shadow.size: 60
window.inactive.shadow.size: 40
window.active.shadow.color: #00000060
window.inactive.shadow.color: #00000040
```

- Add window-rule `ignoreConfigureRequest` to ignore X11 client-side
  configure requests (positioning and resizing). [#1446]
- Support window-rules based on window type: `<windowRule type="">`, where
  type can be for example `NET_WM_WINDOW_TYPE_DESKTOP` for an XWayland
  window. Written-by: @xi @txgk
- Add `none` branch to the `ForEach` action. Written-by: @nicolas3121
  [#1298]

```xml
<action name="ForEach">
  <query identifier="foo"/>
  <then>
    <!-- carry out some action on match -->
  </then>
  <none>
    <!-- carry out some action if there were no matches at all -->
  </none>
</action>
```

- Add -S|--session `<command>` option to start `<command>` on startup and
  to terminate the compositor when <command> exits. This is useful for
  session management as it allows the session client (for example
  `lxqt-session`) to terminate labwc when exiting itself.
- In theme setting color definitions, support inline alpha encoding like
  `#aabbccff`
- Add window-switcher custom field inspired by printf formatting. [#1670]
  Written-by: @droc12345 and @Consolatis

```xml
<windowSwitcher>
  <fields>
    <field content="custom" format="foobar %b %3s %-10o %-20W %-10i%t" width="100%" />
  </fields>
</windowSwitcher>
```

- Support defining window-switcher width as a percentage of output
  (display) width. Written-by: @droc12345

```
osd.window-switcher.width: 75%
```

- Support Openbox compatible pipe-menus. See labwc-menu(5) for usage.
- Add snap-to-edge overlay. Written-by: @tokyo4j. [#1652] [#1702]
  This includes the following new config and theme settings:

```xml
<snapping>
  <overlay>
    <enabled>yes|no</enabled>
    <delay inner="500" outer="500"/>
  </overlay>
</snapping>
```

```
snapping.overlay.[region|edge].bg.enabled: yes|no
snapping.overlay.[region|edge].border.enabled: yes|no
snapping.overlay.[region|edge].bg.color: #8080b380
snapping.overlay.[region|edge].border.width: 1
snapping.overlay.[region|edge].border.color: #ffffff,#000000,#ffffff
```

- Add theme settings listed below for window-switcher preview border.
  Written-by: @tokyo4j

```
osd.window-switcher.preview.border.width: 2
osd.window-switcher.preview.border.color: #ffffff,#00a2ff,#ffffff
```

- Support libinput config option for calibration matrices.
  `<libinput><device><calibrationMatrix>`. Written-by: @SnowNF
- Add new window-switcher field content types `workspace`, `state`,
  `type_short` and `output`. Written-by: @droc12345 [#1623]

```xml
<windowSwitcher allWorkspaces="yes">
  <fields>
    <field content="workspace" width="5%" />
    <field content="state" width="3%" />
    <field content="type_short" width="3%" />
    <field content="output" width="9%" />
    <field content="identifier" width="30%" />
    <field content="title" width="50%" />
  </fields>
</windowSwitcher>
```

- Support input methods (or input method editors, commonly abbreviated
  IMEs) like Fcitx5, using protocols text-input-v3 and input-method-v2.
  This includes IME popups. Written-by: @tokyo4j
- Add `atCursor` attribute to action `ShowMenu` so that a window's
  "client-menu" could optionally be launched at the pointer using a
  keybind as follows:

```xml
<action name="ShowMenu" menu="value" atCursor="yes" />
```

- Support workspace-prefix (`<desktops><prefix>`) for workspace-switcher
  onscreen display when naming workspaces by digits, for example 1, 2, 3
  Written-by: @droc12345
- Process all `*.env` files in an `environment.d` directory alongside and
  in the same way as each potential `environment` file.
- Allow empty variables in `environment` files. In other words, respond to
  variable declarations of the form "VARIABLE=", with no following value,
  by setting the corresponding environment variable as an empty string.
- Add optional headless fallback output that is automatically created when
  no other output exists.  Enable this by setting the environment variable
  `LABWC_FALLBACK_OUTPUT` to the desired output name.  The feature
  benefits applications like wayvnc the most by ensuring that there is
  always an output available to connect to. [#1618]
  Co-Authored-By: Simon Long <simon@raspberrypi.com>
- Optionally show windows on all workspaces in window-switcher.

```xml
<windowSwitcher allWorkspaces="yes">
```

- Handle touch on headerbar using cursor emulate events. [#1550]
  Written-by: @spl237
- Updated dbus activation environment with more environment variables
  (`XCURSOR_SIZE`, `XCURSOR_THEME`, `XDG_SESSION_TYPE`, `LABWC_PID`)
  Written-by: @winerysearch  [#694]
- Run `shutdown` script on exit (equivalent to `autostart` on startup)
- Add `wrap` argument to action `MoveToOutput`. Wrap is disabled by
  default to keep the user interface consistent. Example usage:

```xml
<action name="MoveToOutput" direction="right" wrap="yes" />
```

### Fixed

- Prevent Chromium from crashing when started after a virtual keyboard is
  destroyed. [#1789]
- Fix top-layer not showing when there is a minimized full-screen window
  Written-by: @fberg
- Prevent the following whilst window-switcher cycling [#1640]:
  - Cursor actions on the window previews
  - Request-xdg-activation
  - Foreign toplevel request-activate
  - XWayland request-activate
- Prevent shaded XWayland windows from getting cursor events. [#1753]
- Fix menu-parser use-after-free bug. [#1780]
- Update top layer visibility on map to fix bug with Steam's Big Picture Mode
  window which requests fullscreen before mapping. [#1763]
- Do not update server-side-decoration if window is too small. [#1762]
- Fix crash on `Kill` action with XWayland windows. [#1739]
- Update workspaces on `--reconfigure`. Written-by: @tokyo4j
- Notify idle manager when emulating cursor movement.
- Fix GrowToEdge/ShrinkToEdge action bug caused by clients ignoring the
  requested size, for example a terminal honouring size-hints.
- Fix `assert()` on VT switch. [#1667]
- Ensure titlebar has consistent look when using transparency. [#1684]
- Fix dnd bug where dnd does not finish properly on cursor-button-release
  if there is no surface under the cursor such as on the desktop when no
  background client is running. [#1673]
- Send cursor-button release event to CSD client before finishing window
  dragging to avoid a bug whereby the release event is incorrectly sent to
  a layer-shell client at the end of a drag.
- Validate double-click against SSD part type because clicks on
  different parts of a client in quick succession should not be
  interpreted as a double click. [#1657]
- Fix bug that region overlay is not shown when a modifier key is re-
  pressed.
- Fix workspace-switcher on-screen-display positioning of text using
  right-to-left (RTL) locales. Written-by: @micko01 Issue [#1633]
- Unconstrain xdg-shell popups to usable area (rather than full output) so
  that popups do not cover layer-shell clients such as panels.
  Written-by: @tokyo4j
- Exclude unfocusable XWayland windows (for example notifications and
  floating toolbars) from being processed by wlr-foreign-toplevel protocol
  as these windows should not be shown in taskbars/docks/etc.
- Render text buffers with opaque backgrounds because subpixel text
  rendering over a transparent background does not work properly with
  cairo/pango. [#1631] [#1684]
- Fallback on layout 'us' if a keymap cannot be created for the provided
  `XKB_DEFAULT_LAYOUT`. If keymap still cannot be created, exit with a
  helpful message instead of a segv crash.
- Reload cursor theme and size on reconfigure. This gives instant feedback, but
  only works for server side cursors or clients using the cursor-shape protocol.
  Written-by: @spl237 and @Consolatis. [#1587] [#1619]
- Fix a number of surface-focus related short-comings:
  - Handle cursor-button-press on layer-shell subsurfaces and fix bug in
    `get_cursor_context()` which resulted in layer-surfaces not being
    detected correctly. [#1594]
  - Overhaul the logic for giving keyboard focus to layer-shell clients.
    [#1599] [#1653]
- Fix move/resize bug manifesting itself on touchpad taps with
  `<tapAndDrag>` disabled because libinput sends button press & release
  signals so quickly that `interactive_finish()` is never called.
  Written-by: @tokyo4j
- Include always-on-top windows in window-switcher.
- Make resize flicker free again when running labwc nested (it was a
  regression caused by wlroots 0.17).
- Clean up dbus and systemd activation environments on exit
- Fix `view_get_adjacent_output()` bug resulting in often returning an
  incorrect output when using more than two outputs. [#1582]

### Changed

- Support press-move-release when interacting with the labwc root-menu. [#1750]
- In theme settings, mark color definitions in the format `#rrggbb aaa` as
  deprecated (still supported, but will removed in some future release) in
  favor of the more commonly used `#rrggbbaa`.
- If your `rc.xml` contains a keybind to show menu "client-menu", it will
  be launched at pointer rather than the top-left part of the window. To
  keep the old behaviour, redefine it as follows:

```xml
<keybind key="A-Space">
  <action name="ShowMenu" menu="client-menu" atCursor="No"/>
</keybind>
```

- Change action `MoveToOutput` argument 'name' to 'output' (because 'name'
  is already used by the action itself).  [#1589]

```xml
<action name="MoveToOutput" output="HDMI-A-1"/>
```

- Do not deactivate window when giving keyboard focus to a non-view
  surface such as a popup or layer-shell surface.  This matches Openbox
  behavior.
- Treat Globally Active XWayland windows according to type to fix focus
  issues with IntelliJ IDEA and JDownloader 2. [#1139] [#1341]
  Also revert f6e3527 which allowed re-focus between Globally Active
  XWayland windows of the same PID.
- Only update dbus and systemd activation environments when running on
  the DRM backend or by explicit request using environment variable
  `LABWC_UPDATE_ACTIVATION_ENV`.

## 0.7.1 - 2024-03-01

[0.7.1-commits]

### Added

- Support libinput option sendEventsMode to allow enabling/disabling devices.
  Co-Authored-By: @Sachin-Bhat

```xml
<libinput>
  <device>
    <sendEventsMode>yes|no|disabledOnExternalMouse</sendEventsMode>
  </device>
</libinput>
```

- Add click method libinput option. Written-by: @datMaffin

```xml
<libinput>
  <device>
    <clickMethod>none|buttonAreas|clickfinger</clickMethod>
  </device>
</libinput>
```

- Add `data/labwc.svg` & `data/labwc-symbolic.svg`, and specify icon name
  in labwc.desktop to enable Display Managers to show an icons for labwc.
- Expose output configuration test to clients. For example, this enables
  `wlr-randr --dryrun`
- Add window-edge resistance for interactive moves/resizes and support negative
  strengths to indicate attractive snapping. Written-by: @ahesford

```xml
<resistance>
  <screenEdgeStrength>-20</screenEdgeStrength>
  <windowEdgeStrength>-20</windowEdgeStrength>
</resistance>
```

- Set keyboard layout on reconfigure. [#1407]
- Reset keyboard-layout group (index) for each window on reconfigure if
  the keymap has changed.
- Support merging multiple config files with the --merge-config command
  line option. [#1406]
- Add config option to map touch events to a named output (display).
  Optionally, make this only apply to specific named devices.
  Written-by: @jp7677

```xml
<touch mapToOutput=""/>
<touch deviceName="" mapToOutput=""/>
```

- Add tablet support including:
  - Mapping of tablet to output (display)
  - Emulation of cursor movement and button press/release
  - Configuration of area and rotation
  Written-by: @jp7677 @Consolatis

```xml
<tablet mapToOutput="HDMI-A-1" rotate="90">
  <area top="0.0" left="0.0" width="0.0" height="0.0" />
  <map button="Tip" to="Left" />
  <map button="Stylus" to="Right" />
  <map button="Stylus2" to="Middle" />
</tablet>
```

- Add tearing support. #1390. Written-by: @Ph42oN @ahesford
- Add configuration support for mouse buttons `Side`, `Extra`, `Forward`,
  `Back` and `Task`. Written-by: @jp7677
- config: allow `<libinput><device>` without category attribute to define a
  `default` profile because it is more user-friendly and intuitive.
- Add a configuration option to enable adaptive sync only when an application
  is in fullscreen mode. Written-by: @Ph42oN
- Add `touchpad` libinput device type to increase configuration flexibility,
  for example allowing `naturalScroll` on touchpads, but not on regular pointer
  devices such as mice. Written-by: @jmbaur
- Add actions:
  - `AutoPlace` (by @ahesford)
  - `MoveToOutput`, `FitToOutput` (by @jp7677)
  - `Shade`, `Unshade`, `ToggleShade` (by @ahesford @Consolatis)
- Add config option `<placement><policy>` with supported values `center`,
  `cursor` and `automatic`. The latter minimizes overlap with other windows
  already on screen and is similar to Openbox's smart window placement.
  The placement policies honour `<core><gap>`.
  Written-by: @ahesford [#1312]

```xml
<placement>
  <policy>center|automatic|cursor</policy>
</placement>
```

### Fixed

- Delay popup-unconstrain until after first commit in response to a changed
  wlroots 0.17 interface and to get rid of the error message
  `[types/xdg_shell/wlr_xdg_surface.c:169] A configure is scheduled for an uninitialized xdg_surface`
  [#1372]
- Notify clients about configuration errors when changing output settings.
  [#1528]
- Fix output configuration bug causing compositor crash when refresh rate is
  zero. [#1458]
- Fix disappearing cursor bug on view destruction. [#1393]
- Use used specified config-file (using -c command line option) on
  reconfigure.
- Assign outputs to new views on surface creation instead of mapping, and
  notify the client of the preferred output scale when doing so. This fixes an
  issue with foot: https://codeberg.org/dnkl/foot/issues/1579
  Written-by: @ahesford
- Cancel key repeat on vt change to fix crash on VT change on FreeBSD. [#1424]
- Fix crash when a minimized fullscreen window closes. Written-by: @bi4k8
- Execute menu actions after closing menus so that menu entries can issue
  `wtype` commands to the surface with keyboard-focus. [#1366]
- Try to honor original window geometry on layout changes.
- Fix virtual keyboard bug experienced with `wlrctl keyboard type xyz`. Do not
  process virtual keyboard keycodes (just the keysyms). [#1367]
- Sync xdg-shell client `view->pending` when applying geometry to fix issue
  caused by applications choosing not respond to pending resize requests either
  by ignoring them or substituting alternative sizes (for example, when mpv
  constrains resizes to keep its aspect ratio fixed). Written-by: @ahesford

### Changed

- Make `MoveToCursor` honour `<core><gap>`. [#1494]
- Add `Roll Up/Down` client-menu entry for `ToggleShade`
- When a Wayland-native window is snapped to a screen edges or user-defined
  region, labwc will notify the application that it is "tiled", allowing the
  application to better adapt its rendering to constrained layouts. Windows
  with client-side decorations may respond to these notices by squaring off
  corners and, in some cases, disabling resize abilities. This can be disabled
  with:

```xml
<snapping>
  <notifyClient>never</notifyClient>
</snapping>
```

  or limited to only edge-snapped or only region-snapped windows. See the
  labwc-config(5) manual page for more information.

- When a window is dragged from a snapped position (either a screen edge or a
  user-defined region), the snapped state is now discarded as soon as the
  dragging begins. This means that dragging from a snapped position to a
  maximized state (with the `topMaximize` option enabled) and then
  un-maximizing the window will restore the window to its size and position
  *before* it was snapped. In previous releases, un-maximizing would restore
  the window to its snapped state. To preserve the snapped state of a window
  when maximized, use the Maximize window button or the `ToggleMaximize`
  action.

- The new windowEdgeStrength setting makes windows resist interactive moves and
  resizes across the edges of other windows. This can be disabled with:

```xml
<resistance>
  <windowEdgeStrength>0</windowEdgeStrength>
</resistance>
```

- Run menu actions on button release instead of press.
- Constrain window size to that of usable area when an application is started.
  [#1399]
- Support showing the full `app_id` in the window switcher. Users with a custom
  `windowSwitcher` configuration should use the `trimmed_identifier` field
  label to preserve existing behavior; the `identifier` field now refers to the
  full `app_id`. Consult the labwc-config(5) manual page for more details.
  [#1309]

## 0.7.0 - 2023-12-22

[0.7.0-commits]

The main effort in this release has gone into porting labwc to wlroots 0.17
and tidying up regressions. Nonetheless, it contains a significant number of
additions and fixes as described below.

Should bug fixes be required against `0.6.6` (built with wlroots `0.16`), a
`0.6` branch will be created.

### Added

- Support titlebar hover icons. Written-by: @spl237
- Add theme options osd.workspace-switcher.boxes.{width,height}
  Written-by: @kyak
- Add actions `VirtualOutputAdd` and `VirtualOutputRemove` to control virtual
  outputs. Written-by: @kyak [#1287]
- Teach MoveToEdge to move windows to adjacent outputs.
  Written-by: @ahesford
- Implement `<font place="InactiveWindow">`. Written-by: @ludg1e [#1292]
- Implement cursor-shape-v1 protocol to allow Wayland clients to request a
  buffer for a cursor shape from a compositor. Written-by: @heroin-moose
- Implement fractional-scale-v1 protocol to allow Wayland clients to properly
  scale on outputs with fractional scale factor. Written-by: @heroin-moose
- Add ResizeTo action [#1261]
- Allow going backwards in window-switcher OSD by using arrow-up or arrow-left.
  Written-by: @jp7677
- Add `ToggleOmnipresent` action and add an "Always on Visible Workspace" entry
  for it in the client-menu under the Workspaces submenu. Written-by: @bnason
- Account for space taken up by XWayland clients with `_NET_WM_STRUT_PARTIAL`
  property in the `usable_area` calculation. This increases inter-operability
  with X11 desktop components.
- Set XWayland's `_NET_WORKAREA` property based on usable area. XWayland
  clients use the `_NET_WORKAREA` root window property to determine how much of
  the screen is not covered by panels/docks. The property is used for example
  by Qt to determine areas of the screen that popup menus should not overlap.

### Fixed

- Fix xwayland.c null pointer dereference causing crash with JetBrains CLion.
  [#1352]
- Fix issue with XWayland surfaces completely offscreen not generating commit
  events and therefore preventing them from moving onscreen.
- Do not de-active windows when layer-shell client takes keyboard focus, to
  fix sfwbar minimize action. [#1342]
- Move layer-shell popups from the background layer to the top layer to render
  them above normal windows. Previously this was only done for the bottom
  layer. In support of Raspberry Pi's `pcmanfm --desktop`. [#1293]
- Calculate `usable_area` before positioning clients to ensure it is correct
  before non exclusive-zone layer-shell clients are positioned or resized.
  [#1285]
- Prevent overriding XWayland maximized/fullscreen/tiled geometry to fix an
  issue where some XWayland views (example: xfce4-terminal) do not end up with
  exactly the correct geometry when tiled.

### Changed

- Treat XWayland panel windows as if fixedPosition rule is set
- Use the GTK3 notebook header color as the default active title color
  (small change from `#dddad6` to `#e1dedb`). Written-by: @dimkr

## 0.6.6 - 2023-11-25

[0.6.6-commits]

We do not normally call out contributions by core devs in the changelog,
but a special thanks goes to @jlindgren90 in this release for lots of work
relating to surface focus and keyboard issues, amongst others.

### Added

- Add `fixedPosition` window-rule property to avoid re-positioning windows
  on reserved-output-space changes (determined by *<margin>* settings or
  exclusive layer-shell clients) and to disallow interactive move or
  resize, for example by alt+press.
- Add `Unfocus` action to enable unfocusing windows on desktop click. [#1230]
- Add config option `<keyboard layoutScope="window">` to use per-window
  keyboard layout. [#1076]
- Support separate horizontal and vertical maximize by adding a
  `direction` option to actions Maximize and ToggleMaximize.
- Add actions GrowToEdge and ShrinkToEdge. Written-by: @digint
- Add `snapWindows` option to MoveToEdge action. Written-by: @digint
- Add MoveToCursor action. Written-by: @Arnaudv6
- Add config option `<keyboard><numlock>` to enable Num Lock on startup.
- Support Meta (M), Hyper (H), Mod1, Mod3, Mod4 and Mod5 modifiers in
  keybind definitions. [#1061]
- Add themerc 'titlebar.height' option. Written-by: @mozlima
- Add If and ForEach actions. Written-by: @consus
- Allow referencing the current workspace in actions, for example:

```xml
<action name="SendToDesktop" to="current"/>
```

### Fixed

- Do not reset XWayland window SSD on unminimize
- Keep XWayland stacking order in sync when switching workspaces
- Update top-layer visibility on workspace-switch in order to show
  top-layer layer-shell clients correctly when there is a window in
  fullscreen mode on another workspace. [#1040] [#1158]
- Make interactive window snapping with mouse more intuitive in
  multi-output setups. Written-by: @tokyo4j
- Try to handle missing `set_window_geometry` with Qt apps which
  occasionally fail to call `set_window_geometry` after a configure
  request, but correctly update the actual surface extent. [#1194]
- Update XWayland stacking order when moving a window to the front/back.
- Prevent switching workspaces for always-on-bottom windows. [#1170]
- Fix invisible cursor after wlopm --off && wlopm --on.
- When a session is locked using 'session-lock' protocol, reconfigure for
  output layout changes to avoid incorrect positioning
- Account for window base size in resize indicator so that the displayed
  size exactly matches the terminal grid, for example 80x25.
- The following focus related issues:
  - Allow re-focusing xwayland-unmanaged surfaces in response to pointer
    action (click or movement if focus-follow-mouse is enabled). This
    enables clients such as dmenu, rofi and jgmenu to regain
    keyboard-focus if it was lost to another client.
  - Fix code paths which could lead to a lock-screen losing focus, making
    the session impossible to unlock or another surface to gain focus thus
    breaching the session lock.
  - Only focus topmost view on unmap if unmapped view was focused.
  - Fix `xwayland_surface->data` bug relating to unmanaged surfaces.
  - Fix layer subsurface focus bug to make waybar's minimize-raise work. [#1131]
  - Ignore focus change to unmanaged surface belonging to same PID to fix
    an issue with menus immediately closing in some X11 apps.
  - Avoid focusing xwayland views that do not want focus using the ICCCM
    "Globally Active" input model.
  - Allow re-focus between "globally active" XWayland views of the same
    PID.
  - Assume that views that want decorations also want focus
- The following keyboard and keybind related issues:
  - Send pressed keys correctly when focusing new surface.
  - Refactor handling of pressed/bound keys to send (to client) the
    release events for any pressed key that was not part of a keybind,
    typically because an unrelated non-modifier key was pressed before
    and held during a keybind invocation. [#1091] [#1245]
  - Fix keyboard release event bug after session lock. [#1114]
- Raise xdg and xwayland sub-views correctly relative to other sub-views,
  by letting the relative stacking order between them change.
- Honor initially maximized requests for XWayland views via
  `_NET_WM_STATE`.
- For initially maximized XWayland views, set the stored natural geometry
  to be output-centered.
- Fix regions rounding error sometimes resulting in incorrect gaps
  between regions.

### Changed

- Move floating windows in response to changes in reserved output space
  (determined by *<margin>* settings or exclusive layer-shell clients such
  as panels). Users with window-rules for panels and/or desktops should
  add the `fixedPosition` property to avoid regression. [#1235]
- Restore `SIGPIPE` default handler before exec. [#1209]
- With the introduction of directional Maximize, right-click on the
  maximize button now toggles horizontal maximize, while middle-click
  toggles vertical maximize.
- Make MoveToEdge snap to the next window edge by default rather than
  just the screen edge.
- Comment out variables in `docs/environment` to avoid users using the
  file without editing it and ending up with unwanted settings. [#1011]
- Set `_JAVA_AWT_WM_NONREPARENTING=1` unless already set.
- This release has seen significant refactoring and minor improvements
  with respect to window and surface focus (particular thanks to
  @jlindgren). This work has helped uncover and fix some hard-to-find
  bugs. We don't believe that there are any regressions, but can't say
  for sure.
- Set Num Lock to enabled by default on start up
- Allow switching VT when locked
- Use `fnmatch()` for pattern matching instead of `g_pattern_match_simple()`
  because it is a POSIX-compliant function which has a glob(7) manual page
  for reference.
- Title context is used instead of TitleBar for the default client-menu
  on click. This means that if a button is right-clicked, the client-menu
  will not appear anymore.
- Always switch to the workspace containing the view being focused.

## 0.6.5 - 2023-09-23

[0.6.5-commits]

### Added

- Support png and svg titlebar buttons
- Support on/off boolean configuration values (in addition to true, false,
  yes and no). Written-by: @redtide
- keybinds
  - Allow non-english based keybinds
  - Make keybind agnostic to keyboard layout. [#1069]
  - Add optional layoutDependent argument to only trigger if the
    configured key exists in the currently active keyboard layout.
    `<keybind key="" layoutDependent="">`
  - Fallback on raw keysyms (as if there were no pressed modifier) for
    bindings which do not match against translated keysyms. This allows
    users to define keybinds such as "S-1" rather than "S-exclam". It also
    supports "W-S-Tab".  [#163] [#365] [#992]
- window-rules: add ignoreFocusRequest property
- config: support libinput `<tapAndDrag>` and `<dragLock>`.
  Written-by: @tokyo4j
- Handle keyboard input for menus. [#1058]
- Server-side decoration:
  - Make corners square on maximize
  - Disable border on maximize. [#1044]
- Add window resize indicator and associated `<resize><popupShow>` config
  option
- Add `<theme><keepBorder>` to give `ToggleDecoration` three states:
  (1) disable titlebar; (2) disable whole SSD; and (3) enables whole SSD
  When the keepBorder action is disabled, the old two-state behavior is
  restored. [#813]
- Minimize whole window hierarchy from top to bottom regardless of which
  window requested the minimize. For example, if an 'About' or 'Open File'
  dialog is minimized, its toplevel is minimized also, and vice versa.
- Move window's stacking order with dialogs so that other window cannot be
  positioned between them. Also position xdg popups above their parent
  windows.This is consistent with Gtk3 and Qt5. [#823]

### Fixed

- Clarify in labwc-config(5) that keyboard modifiers can be used for
  mousebinds. [#1075]
- Ensure interactive move/resize ends correctly for CSD clients. [#1053]
- Fix invalid value in `<accelProfile>` falling back as "flat"
- Fix touch bug to avoid jumping when a touch point moves off of a surface
  Written-by: @bi4k8
- Prevent crash with theme setting `osd.window-switcher.width: 0`. [#1050]
- Cancel cursor popup grab on mouse-press outside client itself, for
  example on any part of the server side decoration or the desktop. [#949]
- Prevent cursor press on layer-subsurface from cancelling popup grab [#1030]
- xwayland: fix client request-unmap bug relating to foreign-toplevel handle
- xwayland: fix race condition resulting in map view without surface
- Limit SSD corner radius to the height of the titlebar
- Fix rounded-corner bug producing weird artifacts when very large border
  thickness is used. [#988]
- Ensure `string_prop()` handlers deal with destroying views. [#1082]
- Fix SSD thickness calculation bug relating to titlebar. [#1083]
- common/buf.c:
  - Do not expand `$()` in `buf_expand_shell_variables()`
  - Do not use memcpy for overlapping regions

### Changed

- Use `identifier` for window-switcher field rather than `app_id` to be
  consistent with window rules.

```xml
<windowSwitcher>
  <fields>
    <field content="identifier" width="25%"/>
  </fields>
</windowSwithcer>
```

- Do not expand environment variables in `Exec` action `<command>`
  argument (but still resolve tilde).

## 0.6.4 - 2023-07-14

[0.6.4-commits]

### Added

- Add support for `ext_idle_notify` protocol.
- Window-switcher: [#879] [#969]
  - Set item-height based on font-height
  - Add theme option:
    - osd.window-switcher.width
    - osd.window-switcher.padding
    - osd.window-switcher.item.padding.x
    - osd.window-switcher.item.padding.y
    - osd.window-switcher.item.active.border.width
- Actions:
  - Add `MoveTo`, `ToggleAlwaysOnBottom`.
  - Add `MoveRelative`, `ResizeRelative`. Written-by: @Ph42oN
  - Add option `wrap` for `GoToDesktop` and `SendToDesktop`
- Add config options `<margin>` to override usable area for panels/docks
  which do not support layer-shell protocol.
- Add `number` attribute to `<desktops>` to simplify configuration.
  Written-by: @Sachin-Bhat
- Window rules: [#787] [#933]
  - Add properties: `skipTaskbar` and `skipWindowSwitcher`
  - Add criteria `title` and `matchOnce`

### Fixed

- Support XML CDATA for `<menu><item><action><command>` in order to provide
  backward compatibility with obmenu-generator [#972]
- Call `wlr_xwayland_surface_set_minimized()` on xwayland window (un)minimize
  to fix blank surface after minimizing fullscreen Steam windows. [#958]
- Fix focus at the end of drag-and-drop operation respecting
  `<focus><followMouse>` if enabled. [#939] [#976]
- Render xdg-popups above always-on-top layer.
- Do not render On-Screen-Displays on disabled outputs. [#914]

### Changed

- Make `ToggleKeybinds` applicable only to the window that has keyboard focus
  when the action is executed.

## 0.6.3 - 2023-05-08

[0.6.3-commits]

### Added

- Add `focus.followMouseRequiresMovement` to allow a stricter
  focus-what-is-under-the-cursor configuration. [#862]

- Support window-rules including properties and on-first-map actions.
  Any actions in labwc-actions(5) can be used. Only 'serverDecoration'
  has been added as a property so far. Example config:

```xml
<windowRules>
  <windowRule identifier="some-application">
    <action name="Maximize"/>
  </windowRule>
  <windowRule identifier="foo*" serverDecoration="yes|no"/>
</windowRules>
```

- Support configuration of window switcher field definitions.
  Issues [#852] [#855] [#879]

```xml
<windowSwitcher show="yes" preview="yes" outlines="yes">
  <fields>
    <field content="type" width="25%" />
    <field content="app_id" width="25%" />
    <field content="title" width="50%" />
  </fields>
</windowSwitcher>
```

- Add actions:
  - 'Lower' Written-by: @jech
  - 'Maximize'
- Support ext-session-lock protocol. Helped-by: @heroin-moose
- Handle XWayland unmanaged surface requests for 'activate' and
  'override-redirect'. [#874]
- Add config support for scroll-factor. [#846]
- Support 'follow' attribute for SendToDesktop action. [#841]

### Fixed

- Fix adaptive sync configuration. Helped-by: @heroin-moose [#642]
- Ignore SIGPIPE to fix crash caused by Wayland clients requesting X11
  clipboard but closing the read-fd before/while the X11 clipboard is
  being written to. [#890]
- Ellipsize on-screen-display text
- Validate PID before activating XWayland unmanaged surfaces to check that
  the surface trying to grab focus is actually a child of the topmost
  mapped window.
- Respect cursor constraint hints when cursor movement occurs after
  unlocking the pointer. Written-by: @FuzzyQuills [#872]
- Fix invisible cursor on startup and output loss/restore.
  Reported-by: @Flrian [#820]
- Fix decoration protocol implementation
  - Respect earlier decoration negotiation results via the
    xdg-decoration protocol. Previously setting `<decoration>` to
    `client` would cause applications which prefer server side
    decorations to not have any decorations at all. [#297] [#831]
  - Handle results of kde-server-decoration negotiations
- Fix `<focus><followMouse>` cursor glitches and issues with focus
  switching via Alt-Tab. [#830] [#849]

### Changed

- Make `<windowSwitcher>` a toplevel element rather than a child of
  `<core>`
- Default to follow="true" for SendToDesktop action as per Openbox 3.6
  specification.

## 0.6.2 - 2023-03-20

[0.6.2-commits]

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
  Machines, VNC clients, nested compositors or similar. [#738] [#810]
- Implement cursor constraints (Written-by: @Ph42oN) and lock confinement.
- Support xdg-activation protocol to allow applications to activate
  themselves (e.g. raise to the top and get keyboard focus) if they
  provide a valid `xdg_activation token`.
- Allow clearing key/mouse bindings by using the 'None' action. This
  enables the use of `<default />` and then selectively removing keybinds.
  For example the following could be used to allow using A-Left/Right with
  Firefox.

```xml
<keyboard>
  <default/>
  <keybind key="A-Left"><action name="None" /></keybind>
  <keybind key="A-Right"><action name="None" /></keybind>
</keyboard>
```

### Fixed

- Prevent cursor based region-snapping when starting a move with Alt-Left.
  If region-snapping is wanted in this situation, just press the modifier
  again. [#761]
- Prevent rare crash due to layering move/resize/menu operations. [#817]
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

```xml
<core>
  <cycleViewOSD>yes</cycleViewOSD>
  <cycleViewOutlines>yes</cycleViewOutlines>
  <cycleViewPreview>yes</cycleViewPreview>
</core>
```

## 0.6.1 - 2023-01-29

[0.6.1-commits]

As usual, this release contains lots of refactoring and bug fixes with
particular thanks going to @Consolatis, @jlindgren90, @bi4k8, @Flrian and
@Joshua-Ashton.

### Added

- Add `<regions>` config option allowing the definition of regions to which
  windows can be snapped by keeping a keyboard modifier pressed while dragging
  or by using the SnapToRegion action. Written-by: @Consolatis
- Add `<Kill>` action to send SIGTERM to a client process. Written-by: @bi4k8
- Add config option `<core><reuseOutputMode>` to support flicker free boot
  [#724]. Written-by: @Consolatis
- Enable single-pixel-buffer-v1
- Support theme setting override by reading `<config-dir>/themerc-override`
- Scale down SSD button icons if necessary to allow using larger ones for high
  and mixed DPI usecases. [#609]. Written-by: @Consolatis
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
- Send enter event when new layer surface appears under pointer. [#667]
- Prevent re-focus for always-on-top views when switching workspaces.
  Written-by: @Consolatis
- Make sure a default libinput category always exists to avoid devices not
  being configured is some instances. Written-by: @jlindgren90
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
- Check for modifiers when merging mousebinds. [#630]
- Handle layer-shell exclusive and on-demand keyboard-interactivity
  correctly, and thus support xfce4-panel better. [#704] [#725]
- Only overwrite wlroots's automatic layout when necessary.

### Changed

- Filter out `wp_drm_lease_device` from Xwayland to avoid Electron apps such as
  VS Code and Discord lagging over time. [#553] Written-by: @Joshua-Ashton
- Do not switch output on SnapToEdge if view is maximized. Written-by: @Flrian

## 0.6.0 - 2022-11-17

[0.6.0-commits]

This release contains significant refactoring to use the wlroots
scene-graph API. This touches many areas of the code, particularly
rendering, server-side-decoration, the layer-shell implementation and the
menu. Many thanks to @Consolatis for doing most of the heavy lifting with
this.

Noteworthy, related changes include:

- The use of a buffer implementation instead of using `wlr_texture`. It
  handles both images and fonts, and scales according to output scale.
- The use of node-descriptors to assign roles to `wlr_scene_nodes` in order
  to simplify the code.
- Improving the "Debug" action to print scene-graph trees

A large number of bugs and regressions have been fixed following the
re-factoring, too many to list here, but we are grateful to all who have
reported, tested and fixed issues. Particular mentions go to @bi4k8,
@flrian, @heroin-moose, @jlindgren90, @Joshua-Ashton, @01micko and @skerit

### Added

- Set environment variable `LABWC_PID` to the pid of the compositor so that
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
  [#416]. Written-by: @Consolatis
- Add config option `<core><cycleViewOutlines>` to enable/disable preview
  of outlines. Written-by: @Flrian
- Render submenu arrows
- Allow highest level menu definitions - typically used for root-menu and
  client-menu - to be defined without label attribute, for example like this:
  `<openbox_menu><menu id="root-menu">...</menu></openbox>`. [#472]
- Allow xdg-desktop-portal-wlr to work out of the box by initializing dbus
  and systemd activation environment. This enables for example OBS Studio
  to work with no user configuration. If systemd or dbus is not available
  the environment update will fail gracefully. [#461]
  Written-by: @Joshua-Ashton and @Consolatis
- Workspaces. Written-by: @Consolatis
- presentation-time protocol
- Native language support for client-menus. Written-by: @01micko
- Touch support. Written-by: @bi4k8
- `drm_lease_v1` for VR to work and leasing of desktop displays.
  Written-by: Joshua Ashton
- ToggleAlwaysOnTop action. Written-by: @Consolatis
- Command line option -C to specify config directory
- Theme options osd.border.color and osd.border.width. Written-by: @Consolatis
- Menu `<separator />` and associated theme options:
  menu.separator.width, menu.separator.padding.width,
  menu.separator.padding.height and menu.separator.color
- Adjust maximized and tiled windows according to `usable_area` taking
  into account exclusive layer-shell clients. Written-by: @Consolatis
- Restore natural geometry when moving tiled/maximized window
  [#391] Written-by: @Consolatis
- Improve action implementation to take a list of arguments in preparation
  for actions with multiple arguments. Written-by: @Consolatis

### Fixed

- Remove unwanted gap when initially (on map) positioning windows larger
  than output usable area [#403]
- Prevent setting cursor icon on drag. Written-by: @Consolatis [#549]
- Fix bugs relating to sending matching pairs of press and release
  keycodes to clients when using keybinds. Also fix related key-repeat
  bug. [#510]
- Fix `wlr_output_cursor` initialization bug on new output.
  Written-by: @jlindgren90
- Show correct cursor for resize action triggered by keybind.
  Written-by: @jlindgren
- Fix bug which manifest itself when keeping button pressed in GTK3 menu
  and firefox context menu. Written-by: @jlindgren90
- Enable tap be default on non-touch devices (which some laptop trackpads
  apparently are)
- Handle missing cursor theme [#246] Written-by: @Consolatis
- Fix various surface synchronization, stacking, positioning and focus
  issues, including those related to both xwayland, scroll/drag events
  and also [#526] [#483]
- On first map, do not center xwayland views with explicitly specified
  position. Written-by: @jlindgren90
- Give keyboard focus back to topmost mapped view when unmapping topmost
  xwayland unmanaged surfaces, such as dmenu. Written-by: @Consolatis.
- Fix mousebind ordering and replace earlier mousebinds by later ones
  Written-by: @Consolatis
- Fix various bugs associated with destroying/disabling outputs, including
  [#497]
- Hide Alt-Tab switcher when canceling via Escape. @jlindgren90
- (Re)set seat when xwayland is ready (because wlroots reset the seat
  assigned to xwayland to NULL whenever Xwayland terminates).
  [#166] [#444] Written-by: @Consolatis. Helped-by: @droc12345
- Increase File Descriptor (FD) limit to max because a compositor has to
  handle many: client connections, DMA-BUFs, `wl_data_device` pipes and so on.
  Fixes client freeze/crashes (swaywm/sway#6642). Written-by: @Joshua-Ashton
- Fix crash when creating a cursor constraint and there is no currently
  focused view.
- Gracefully handle dying client during interactive move.
  Written-by: @Consolatis
- Dynamically adjust server-side-decoration invisible resize areas based
  on `usable_area` to ensure that cursor events are sent to clients such as
  panels in preference to grabbing window edges. [#265]
  Written-by: @Consolatis
- Always position submenus inside output extents. [#276]
  Written-by: @Consolatis
- Do not crash when changing TTY. Written-by: @bi4k8
- Set wlroots.wrap to a specific commit rather than master because it
  enables labwc commits to be checked out and build without manually
  having to find the right wlroots commit if there are upstream breaking
  changes.
- Increase accuracy of window center-alignment, taking into account
  `usable_area` and window decoration. Also, top/left align if window is
  bigger than usable area.
- Handle view-destruction during alt-tab cycling.
  Written-by: @Joshua-Ashton
- Survive all outputs being disabled
- Check that double-clicks are on the same window. Written-by: yizixiao
- Set xdg-shell window position before maximize on first map so that the
  unmaximized geometry is known when started in maximized mode.
  [#305] Reported-by: @01micko
- Support `<menu><item><action name="Execute"><execute>`
  `<execute>` is a deprecated name for `<command>`, but is supported for
  backward compatibility with old menu-generators.
- Keep xwayland-shell SSD state on unmap/map cycle.
  Written-by: @Consolatis
- Prevent segfault on missing direction arguments. Reported-by: @flrian
- Fix keybind insertion order to restore intended behavior of keybinds
  set by `<default />`. Written-by: @Consolatis
- Ensure client-menu actions are always applied on window they belong to
  [#380]. Written-by: @Consolatis
- Keep window margin in sync when toggling decorations.
  Written-by: @Consolatis
- Fix handling of client-initiated configure requests.
  Written-by: @jlindgren90
- Always react to new output configuration. Reported-by @heroin-moose and
  Written-by: @Consolatis
- Fix bug in environment variable expansion by allowing underscores to be
  part of the variable names. [#439]
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

## 0.5.3 - 2022-07-15

[0.5.3-commits]

### Added

- wlr-output-power-management protocol to enable clients such as wlopm
  Written-by: @bi4k8

### Fixed

- Call foreign-toplevel-destroy when unmapping xwayland surfaces because
  some xwayland clients leave unmapped child views around. Although
  `handle_destroy()` is not called for these, we have to call
  foreign-toplevel-destroy to avoid clients such as panels incorrectly
  showing them.
- Handle xwayland `set_override_redirect` events to fix weird behaviour
  with gitk menus and rofi.
- Re-focus parent surface on unmapping xwayland unmanaged surfaces
  [#352] relating to JetBrains and Intellij focus issues
  Written-by: Jelle De Loecker
- Do not segfault on missing drag icon. Written-by: @Consolatis
- Fix windows erratically sticking to edges during move/resize [#331] [#309]

## 0.5.2 - 2022-05-17

[0.5.2-commits]

This is a minor bugfix release mostly to ease packaging.

### Fixed

- Properly use system provided wlroots. Written-by: @eli-schwartz

## 0.5.1 - 2022-04-08

[0.5.1-commits]

### Added

- Honour size increments from `WM_SIZE_HINTS`, for example to allow
  xwayland terminal emulators to be resized to a width/height evenly
  divisible by the cell size. Written-by: @jlindgren90
- Implement cursor input for overlay popups. Written-by: @Consolatis

### Fixed

- Do not raise xwayland windows when deactivating [#270]
  Written-by: @Consolatis
- Restore drag mouse-bindings and proper double-click [#258] [#259]
  Written-by: @Consolatis
- Implement cursor input for unmanaged xwayland surfaces outside their
  parent view. Without this menus extending outside the main application
  window do not receive mouse input. Written-by: @jlindgren90
- Allow dragging scrollbar or selecting text even when moving cursor
  outside of the window [#241]. Written-by: @Consolatis
- Fix positioning of xwayland views with multiple queued configure
  events. Written-by: @Consolatis
- Force a pointer enter event on the surface below the cursor when
  cycling views [#162]. Written-by: @Consolatis
- Fix qt application crash on touchpad scroll [#225]
  Written-by: @Consolatis

## 0.5.0 - 2022-02-18

[0.5.0-commits]

As usual, this release contains a bunch of fixes and improvements, of
which the most notable feature-type changes are listed below. A big
thank you to @ARDiDo, @Consolatis and @jlindgren90 for much of the hard
work.

### Added

- Render overlay layer popups to support sfwbar [#239]
- Support HiDPI on-screen-display images for outputs with different scales
- Reload environment variables on SIGHUP [#227]
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

## 0.4.0 - 2021-12-31

[0.4.0-commits]

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

## 0.3.0 - 2021-06-28

[0.3.0-commits]

Compile with wlroots 0.14.0

### Added

- Add config options `<focus><followMouse>` and `<focus><raiseOnFocus>`
  (provided-by: Mikhail Kshevetskiy)
- Do not use Clearlooks-3.4 theme by default, just use built-in theme
- Fix bug which triggered Qt application segfault

## 0.2.0 - 2021-04-15

[0.2.0-commits]

Compile with wlroots 0.13.0

### Added

- Support wlr-output-management protocol for setting output position, scale
  and orientation with kanshi or similar
- Support server side decoration rounded corners
- Change built-in theme to match default GTK style
- Add labwc-environment(5)
- Call `wlr_output_enable_adaptive_sync()` if `LABWC_ADAPTIVE_SYNC` set

## 0.1.0 - 2021-03-05

[0.1.0-commits]

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
[unreleased-commits]: https://github.com/labwc/labwc/compare/0.9.2...HEAD
[0.9.2-commits]: https://github.com/labwc/labwc/compare/0.9.1...0.9.2
[0.9.1-commits]: https://github.com/labwc/labwc/compare/0.9.0...0.9.1
[0.9.0-commits]: https://github.com/labwc/labwc/compare/0.8.4...0.9.0
[0.8.4-commits]: https://github.com/labwc/labwc/compare/0.8.3...0.8.4
[0.8.3-commits]: https://github.com/labwc/labwc/compare/0.8.2...0.8.3
[0.8.2-commits]: https://github.com/labwc/labwc/compare/0.8.1...0.8.2
[0.8.1-commits]: https://github.com/labwc/labwc/compare/0.8.0...0.8.1
[0.8.0-commits]: https://github.com/labwc/labwc/compare/0.7.3...0.8.0
[0.7.4-commits]: https://github.com/labwc/labwc/compare/0.7.3...0.7.4
[0.7.3-commits]: https://github.com/labwc/labwc/compare/0.7.2...0.7.3
[0.7.2-commits]: https://github.com/labwc/labwc/compare/0.7.1...0.7.2
[0.7.1-commits]: https://github.com/labwc/labwc/compare/0.7.0...0.7.1
[0.7.0-commits]: https://github.com/labwc/labwc/compare/0.6.6...0.7.0
[0.6.6-commits]: https://github.com/labwc/labwc/compare/0.6.5...0.6.6
[0.6.5-commits]: https://github.com/labwc/labwc/compare/0.6.4...0.6.5
[0.6.4-commits]: https://github.com/labwc/labwc/compare/0.6.3...0.6.4
[0.6.3-commits]: https://github.com/labwc/labwc/compare/0.6.2...0.6.3
[0.6.2-commits]: https://github.com/labwc/labwc/compare/0.6.1...0.6.2
[0.6.1-commits]: https://github.com/labwc/labwc/compare/0.6.0...0.6.1
[0.6.0-commits]: https://github.com/labwc/labwc/compare/0.5.0...0.6.0
[0.5.3-commits]: https://github.com/labwc/labwc/compare/0.5.2...0.5.3
[0.5.2-commits]: https://github.com/labwc/labwc/compare/0.5.1...0.5.2
[0.5.1-commits]: https://github.com/labwc/labwc/compare/0.5.0...0.5.1
[0.5.0-commits]: https://github.com/labwc/labwc/compare/0.4.0...0.5.0
[0.4.0-commits]: https://github.com/labwc/labwc/compare/0.3.0...0.4.0
[0.3.0-commits]: https://github.com/labwc/labwc/compare/0.2.0...0.3.0
[0.2.0-commits]: https://github.com/labwc/labwc/compare/0.1.0...0.2.0
[0.1.0-commits]: https://github.com/labwc/labwc/compare/081339e...0.1.0

[#162]: https://github.com/labwc/labwc/pull/162
[#163]: https://github.com/labwc/labwc/pull/163
[#166]: https://github.com/labwc/labwc/pull/166
[#225]: https://github.com/labwc/labwc/pull/225
[#227]: https://github.com/labwc/labwc/pull/227
[#239]: https://github.com/labwc/labwc/pull/239
[#241]: https://github.com/labwc/labwc/pull/241
[#246]: https://github.com/labwc/labwc/pull/246
[#258]: https://github.com/labwc/labwc/pull/258
[#259]: https://github.com/labwc/labwc/pull/259
[#265]: https://github.com/labwc/labwc/pull/265
[#270]: https://github.com/labwc/labwc/pull/270
[#276]: https://github.com/labwc/labwc/pull/276
[#297]: https://github.com/labwc/labwc/pull/297
[#305]: https://github.com/labwc/labwc/pull/305
[#309]: https://github.com/labwc/labwc/pull/309
[#331]: https://github.com/labwc/labwc/pull/331
[#352]: https://github.com/labwc/labwc/pull/352
[#365]: https://github.com/labwc/labwc/pull/365
[#380]: https://github.com/labwc/labwc/pull/380
[#391]: https://github.com/labwc/labwc/pull/391
[#403]: https://github.com/labwc/labwc/pull/403
[#416]: https://github.com/labwc/labwc/pull/416
[#439]: https://github.com/labwc/labwc/pull/439
[#444]: https://github.com/labwc/labwc/pull/444
[#461]: https://github.com/labwc/labwc/pull/461
[#472]: https://github.com/labwc/labwc/pull/472
[#483]: https://github.com/labwc/labwc/pull/483
[#497]: https://github.com/labwc/labwc/pull/497
[#510]: https://github.com/labwc/labwc/pull/510
[#526]: https://github.com/labwc/labwc/pull/526
[#549]: https://github.com/labwc/labwc/pull/549
[#553]: https://github.com/labwc/labwc/pull/553
[#609]: https://github.com/labwc/labwc/pull/609
[#630]: https://github.com/labwc/labwc/pull/630
[#642]: https://github.com/labwc/labwc/pull/642
[#667]: https://github.com/labwc/labwc/pull/667
[#694]: https://github.com/labwc/labwc/pull/694
[#704]: https://github.com/labwc/labwc/pull/704
[#724]: https://github.com/labwc/labwc/pull/724
[#725]: https://github.com/labwc/labwc/pull/725
[#738]: https://github.com/labwc/labwc/pull/738
[#761]: https://github.com/labwc/labwc/pull/761
[#787]: https://github.com/labwc/labwc/pull/787
[#810]: https://github.com/labwc/labwc/pull/810
[#813]: https://github.com/labwc/labwc/pull/813
[#817]: https://github.com/labwc/labwc/pull/817
[#820]: https://github.com/labwc/labwc/pull/820
[#823]: https://github.com/labwc/labwc/pull/823
[#830]: https://github.com/labwc/labwc/pull/830
[#831]: https://github.com/labwc/labwc/pull/831
[#841]: https://github.com/labwc/labwc/pull/841
[#846]: https://github.com/labwc/labwc/pull/846
[#849]: https://github.com/labwc/labwc/pull/849
[#852]: https://github.com/labwc/labwc/pull/852
[#855]: https://github.com/labwc/labwc/pull/855
[#862]: https://github.com/labwc/labwc/pull/862
[#872]: https://github.com/labwc/labwc/pull/872
[#874]: https://github.com/labwc/labwc/pull/874
[#879]: https://github.com/labwc/labwc/pull/879
[#890]: https://github.com/labwc/labwc/pull/890
[#914]: https://github.com/labwc/labwc/pull/914
[#933]: https://github.com/labwc/labwc/pull/933
[#939]: https://github.com/labwc/labwc/pull/939
[#949]: https://github.com/labwc/labwc/pull/949
[#958]: https://github.com/labwc/labwc/pull/958
[#969]: https://github.com/labwc/labwc/pull/969
[#972]: https://github.com/labwc/labwc/pull/972
[#976]: https://github.com/labwc/labwc/pull/976
[#988]: https://github.com/labwc/labwc/pull/988
[#992]: https://github.com/labwc/labwc/pull/992
[#1011]: https://github.com/labwc/labwc/pull/1011
[#1030]: https://github.com/labwc/labwc/pull/1030
[#1040]: https://github.com/labwc/labwc/pull/1040
[#1044]: https://github.com/labwc/labwc/pull/1044
[#1050]: https://github.com/labwc/labwc/pull/1050
[#1053]: https://github.com/labwc/labwc/pull/1053
[#1058]: https://github.com/labwc/labwc/pull/1058
[#1061]: https://github.com/labwc/labwc/pull/1061
[#1069]: https://github.com/labwc/labwc/pull/1069
[#1075]: https://github.com/labwc/labwc/pull/1075
[#1076]: https://github.com/labwc/labwc/pull/1076
[#1082]: https://github.com/labwc/labwc/pull/1082
[#1083]: https://github.com/labwc/labwc/pull/1083
[#1091]: https://github.com/labwc/labwc/pull/1091
[#1114]: https://github.com/labwc/labwc/pull/1114
[#1131]: https://github.com/labwc/labwc/pull/1131
[#1139]: https://github.com/labwc/labwc/pull/1139
[#1142]: https://github.com/labwc/labwc/pull/1142
[#1153]: https://github.com/labwc/labwc/pull/1153
[#1154]: https://github.com/labwc/labwc/pull/1154
[#1158]: https://github.com/labwc/labwc/pull/1158
[#1170]: https://github.com/labwc/labwc/pull/1170
[#1194]: https://github.com/labwc/labwc/pull/1194
[#1209]: https://github.com/labwc/labwc/pull/1209
[#1230]: https://github.com/labwc/labwc/pull/1230
[#1235]: https://github.com/labwc/labwc/pull/1235
[#1245]: https://github.com/labwc/labwc/pull/1245
[#1261]: https://github.com/labwc/labwc/pull/1261
[#1278]: https://github.com/labwc/labwc/pull/1278
[#1285]: https://github.com/labwc/labwc/pull/1285
[#1287]: https://github.com/labwc/labwc/pull/1287
[#1292]: https://github.com/labwc/labwc/pull/1292
[#1293]: https://github.com/labwc/labwc/pull/1293
[#1298]: https://github.com/labwc/labwc/pull/1298
[#1309]: https://github.com/labwc/labwc/pull/1309
[#1312]: https://github.com/labwc/labwc/pull/1312
[#1341]: https://github.com/labwc/labwc/pull/1341
[#1342]: https://github.com/labwc/labwc/pull/1342
[#1352]: https://github.com/labwc/labwc/pull/1352
[#1366]: https://github.com/labwc/labwc/pull/1366
[#1367]: https://github.com/labwc/labwc/pull/1367
[#1372]: https://github.com/labwc/labwc/pull/1372
[#1393]: https://github.com/labwc/labwc/pull/1393
[#1399]: https://github.com/labwc/labwc/pull/1399
[#1406]: https://github.com/labwc/labwc/pull/1406
[#1407]: https://github.com/labwc/labwc/pull/1407
[#1424]: https://github.com/labwc/labwc/pull/1424
[#1446]: https://github.com/labwc/labwc/pull/1446
[#1458]: https://github.com/labwc/labwc/pull/1458
[#1494]: https://github.com/labwc/labwc/pull/1494
[#1503]: https://github.com/labwc/labwc/pull/1503
[#1528]: https://github.com/labwc/labwc/pull/1528
[#1529]: https://github.com/labwc/labwc/pull/1529
[#1550]: https://github.com/labwc/labwc/pull/1550
[#1582]: https://github.com/labwc/labwc/pull/1582
[#1587]: https://github.com/labwc/labwc/pull/1587
[#1588]: https://github.com/labwc/labwc/pull/1588
[#1589]: https://github.com/labwc/labwc/pull/1589
[#1594]: https://github.com/labwc/labwc/pull/1594
[#1599]: https://github.com/labwc/labwc/pull/1599
[#1618]: https://github.com/labwc/labwc/pull/1618
[#1619]: https://github.com/labwc/labwc/pull/1619
[#1623]: https://github.com/labwc/labwc/pull/1623
[#1631]: https://github.com/labwc/labwc/pull/1631
[#1633]: https://github.com/labwc/labwc/pull/1633
[#1640]: https://github.com/labwc/labwc/pull/1640
[#1652]: https://github.com/labwc/labwc/pull/1652
[#1653]: https://github.com/labwc/labwc/pull/1653
[#1657]: https://github.com/labwc/labwc/pull/1657
[#1667]: https://github.com/labwc/labwc/pull/1667
[#1670]: https://github.com/labwc/labwc/pull/1670
[#1673]: https://github.com/labwc/labwc/pull/1673
[#1678]: https://github.com/labwc/labwc/pull/1678
[#1684]: https://github.com/labwc/labwc/pull/1684
[#1702]: https://github.com/labwc/labwc/pull/1702
[#1716]: https://github.com/labwc/labwc/pull/1716
[#1731]: https://github.com/labwc/labwc/pull/1731
[#1733]: https://github.com/labwc/labwc/pull/1733
[#1739]: https://github.com/labwc/labwc/pull/1739
[#1750]: https://github.com/labwc/labwc/pull/1750
[#1753]: https://github.com/labwc/labwc/pull/1753
[#1760]: https://github.com/labwc/labwc/pull/1760
[#1762]: https://github.com/labwc/labwc/pull/1762
[#1763]: https://github.com/labwc/labwc/pull/1763
[#1768]: https://github.com/labwc/labwc/pull/1768
[#1774]: https://github.com/labwc/labwc/pull/1774
[#1780]: https://github.com/labwc/labwc/pull/1780
[#1784]: https://github.com/labwc/labwc/pull/1784
[#1789]: https://github.com/labwc/labwc/pull/1789
[#1800]: https://github.com/labwc/labwc/pull/1800
[#1811]: https://github.com/labwc/labwc/pull/1811
[#1816]: https://github.com/labwc/labwc/pull/1816
[#1817]: https://github.com/labwc/labwc/pull/1817
[#1823]: https://github.com/labwc/labwc/pull/1823
[#1831]: https://github.com/labwc/labwc/pull/1831
[#1838]: https://github.com/labwc/labwc/pull/1838
[#1853]: https://github.com/labwc/labwc/pull/1853
[#1858]: https://github.com/labwc/labwc/pull/1858
[#1861]: https://github.com/labwc/labwc/pull/1861
[#1863]: https://github.com/labwc/labwc/pull/1863
[#1872]: https://github.com/labwc/labwc/pull/1872
[#1878]: https://github.com/labwc/labwc/pull/1878
[#1882]: https://github.com/labwc/labwc/pull/1882
[#1888]: https://github.com/labwc/labwc/pull/1888
[#1890]: https://github.com/labwc/labwc/pull/1890
[#1895]: https://github.com/labwc/labwc/pull/1895
[#1907]: https://github.com/labwc/labwc/pull/1907
[#1915]: https://github.com/labwc/labwc/pull/1915
[#1919]: https://github.com/labwc/labwc/pull/1919
[#1926]: https://github.com/labwc/labwc/pull/1926
[#1927]: https://github.com/labwc/labwc/pull/1927
[#1936]: https://github.com/labwc/labwc/pull/1936
[#1938]: https://github.com/labwc/labwc/pull/1938
[#1940]: https://github.com/labwc/labwc/pull/1940
[#1941]: https://github.com/labwc/labwc/pull/1941
[#1950]: https://github.com/labwc/labwc/pull/1950
[#1952]: https://github.com/labwc/labwc/pull/1952
[#1954]: https://github.com/labwc/labwc/pull/1954
[#1956]: https://github.com/labwc/labwc/pull/1956
[#1961]: https://github.com/labwc/labwc/pull/1961
[#1962]: https://github.com/labwc/labwc/pull/1962
[#1963]: https://github.com/labwc/labwc/pull/1963
[#1965]: https://github.com/labwc/labwc/pull/1965
[#1971]: https://github.com/labwc/labwc/pull/1971
[#1988]: https://github.com/labwc/labwc/pull/1988
[#1989]: https://github.com/labwc/labwc/pull/1989
[#1996]: https://github.com/labwc/labwc/pull/1996
[#1997]: https://github.com/labwc/labwc/pull/1997
[#2003]: https://github.com/labwc/labwc/pull/2003
[#2007]: https://github.com/labwc/labwc/pull/2007
[#2009]: https://github.com/labwc/labwc/pull/2009
[#2010]: https://github.com/labwc/labwc/pull/2010
[#2016]: https://github.com/labwc/labwc/pull/2016
[#2020]: https://github.com/labwc/labwc/pull/2020
[#2021]: https://github.com/labwc/labwc/pull/2021
[#2030]: https://github.com/labwc/labwc/pull/2030
[#2040]: https://github.com/labwc/labwc/pull/2040
[#2041]: https://github.com/labwc/labwc/pull/2041
[#2052]: https://github.com/labwc/labwc/pull/2052
[#2055]: https://github.com/labwc/labwc/pull/2055
[#2056]: https://github.com/labwc/labwc/pull/2056
[#2057]: https://github.com/labwc/labwc/pull/2057
[#2060]: https://github.com/labwc/labwc/pull/2060
[#2061]: https://github.com/labwc/labwc/pull/2061
[#2069]: https://github.com/labwc/labwc/pull/2069
[#2072]: https://github.com/labwc/labwc/pull/2072
[#2075]: https://github.com/labwc/labwc/pull/2075
[#2079]: https://github.com/labwc/labwc/pull/2079
[#2088]: https://github.com/labwc/labwc/pull/2088
[#2091]: https://github.com/labwc/labwc/pull/2091
[#2094]: https://github.com/labwc/labwc/pull/2094
[#2097]: https://github.com/labwc/labwc/pull/2097
[#2100]: https://github.com/labwc/labwc/pull/2100
[#2101]: https://github.com/labwc/labwc/pull/2101
[#2102]: https://github.com/labwc/labwc/pull/2102
[#2105]: https://github.com/labwc/labwc/pull/2105
[#2116]: https://github.com/labwc/labwc/pull/2116
[#2118]: https://github.com/labwc/labwc/pull/2118
[#2127]: https://github.com/labwc/labwc/pull/2127
[#2128]: https://github.com/labwc/labwc/pull/2128
[#2150]: https://github.com/labwc/labwc/pull/2150
[#2152]: https://github.com/labwc/labwc/pull/2152
[#2154]: https://github.com/labwc/labwc/pull/2154
[#2166]: https://github.com/labwc/labwc/pull/2166
[#2167]: https://github.com/labwc/labwc/pull/2167
[#2174]: https://github.com/labwc/labwc/pull/2174
[#2189]: https://github.com/labwc/labwc/pull/2189
[#2190]: https://github.com/labwc/labwc/pull/2190
[#2191]: https://github.com/labwc/labwc/pull/2191
[#2196]: https://github.com/labwc/labwc/pull/2196
[#2205]: https://github.com/labwc/labwc/pull/2205
[#2207]: https://github.com/labwc/labwc/pull/2207
[#2221]: https://github.com/labwc/labwc/pull/2221
[#2224]: https://github.com/labwc/labwc/pull/2224
[#2225]: https://github.com/labwc/labwc/pull/2225
[#2226]: https://github.com/labwc/labwc/pull/2226
[#2231]: https://github.com/labwc/labwc/pull/2231
[#2234]: https://github.com/labwc/labwc/pull/2234
[#2238]: https://github.com/labwc/labwc/pull/2238
[#2239]: https://github.com/labwc/labwc/pull/2239
[#2244]: https://github.com/labwc/labwc/pull/2244
[#2245]: https://github.com/labwc/labwc/pull/2245
[#2249]: https://github.com/labwc/labwc/pull/2249
[#2250]: https://github.com/labwc/labwc/pull/2250
[#2257]: https://github.com/labwc/labwc/pull/2257
[#2266]: https://github.com/labwc/labwc/pull/2266
[#2273]: https://github.com/labwc/labwc/pull/2273
[#2274]: https://github.com/labwc/labwc/pull/2274
[#2276]: https://github.com/labwc/labwc/pull/2276
[#2277]: https://github.com/labwc/labwc/pull/2277
[#2291]: https://github.com/labwc/labwc/pull/2291
[#2295]: https://github.com/labwc/labwc/pull/2295
[#2313]: https://github.com/labwc/labwc/pull/2313
[#2319]: https://github.com/labwc/labwc/pull/2319
[#2321]: https://github.com/labwc/labwc/pull/2321
[#2325]: https://github.com/labwc/labwc/pull/2325
[#2326]: https://github.com/labwc/labwc/pull/2326
[#2329]: https://github.com/labwc/labwc/pull/2329
[#2331]: https://github.com/labwc/labwc/pull/2331
[#2335]: https://github.com/labwc/labwc/pull/2335
[#2336]: https://github.com/labwc/labwc/pull/2336
[#2337]: https://github.com/labwc/labwc/pull/2337
[#2338]: https://github.com/labwc/labwc/pull/2338
[#2345]: https://github.com/labwc/labwc/pull/2345
[#2356]: https://github.com/labwc/labwc/pull/2356
[#2360]: https://github.com/labwc/labwc/pull/2360
[#2361]: https://github.com/labwc/labwc/pull/2361
[#2363]: https://github.com/labwc/labwc/pull/2363
[#2365]: https://github.com/labwc/labwc/pull/2365
[#2371]: https://github.com/labwc/labwc/pull/2371
[#2376]: https://github.com/labwc/labwc/pull/2376
[#2377]: https://github.com/labwc/labwc/pull/2377
[#2379]: https://github.com/labwc/labwc/pull/2379
[#2380]: https://github.com/labwc/labwc/pull/2380
[#2388]: https://github.com/labwc/labwc/pull/2388
[#2398]: https://github.com/labwc/labwc/pull/2398
[#2400]: https://github.com/labwc/labwc/pull/2400
[#2408]: https://github.com/labwc/labwc/pull/2408
[#2409]: https://github.com/labwc/labwc/pull/2409
[#2412]: https://github.com/labwc/labwc/pull/2412
[#2414]: https://github.com/labwc/labwc/pull/2414
[#2420]: https://github.com/labwc/labwc/pull/2420
[#2437]: https://github.com/labwc/labwc/pull/2437
[#2443]: https://github.com/labwc/labwc/pull/2443
[#2451]: https://github.com/labwc/labwc/pull/2451
[#2453]: https://github.com/labwc/labwc/pull/2453
[#2455]: https://github.com/labwc/labwc/pull/2455
[#2456]: https://github.com/labwc/labwc/pull/2456
[#2458]: https://github.com/labwc/labwc/pull/2458
[#2460]: https://github.com/labwc/labwc/pull/2460
[#2469]: https://github.com/labwc/labwc/pull/2469
[#2473]: https://github.com/labwc/labwc/pull/2473
[#2477]: https://github.com/labwc/labwc/pull/2477
[#2483]: https://github.com/labwc/labwc/pull/2483
[#2486]: https://github.com/labwc/labwc/pull/2486
[#2488]: https://github.com/labwc/labwc/pull/2488
[#2495]: https://github.com/labwc/labwc/pull/2495
[#2498]: https://github.com/labwc/labwc/pull/2498
[#2499]: https://github.com/labwc/labwc/pull/2499
[#2509]: https://github.com/labwc/labwc/pull/2509
[#2511]: https://github.com/labwc/labwc/pull/2511
[#2513]: https://github.com/labwc/labwc/pull/2513
[#2516]: https://github.com/labwc/labwc/pull/2516
[#2518]: https://github.com/labwc/labwc/pull/2518
[#2524]: https://github.com/labwc/labwc/pull/2524
[#2529]: https://github.com/labwc/labwc/pull/2529
[#2537]: https://github.com/labwc/labwc/pull/2537
[#2539]: https://github.com/labwc/labwc/pull/2539
[#2542]: https://github.com/labwc/labwc/pull/2542
[#2547]: https://github.com/labwc/labwc/pull/2547
[#2550]: https://github.com/labwc/labwc/pull/2550
[#2560]: https://github.com/labwc/labwc/pull/2560
[#2563]: https://github.com/labwc/labwc/pull/2563
[#2574]: https://github.com/labwc/labwc/pull/2574
[#2577]: https://github.com/labwc/labwc/pull/2577
[#2578]: https://github.com/labwc/labwc/pull/2578
[#2580]: https://github.com/labwc/labwc/pull/2580
[#2599]: https://github.com/labwc/labwc/pull/2599
[#2601]: https://github.com/labwc/labwc/pull/2601
[#2602]: https://github.com/labwc/labwc/pull/2602
[#2608]: https://github.com/labwc/labwc/pull/2608
[#2610]: https://github.com/labwc/labwc/pull/2610
[#2613]: https://github.com/labwc/labwc/pull/2613
[#2617]: https://github.com/labwc/labwc/pull/2617
[#2619]: https://github.com/labwc/labwc/pull/2619
[#2621]: https://github.com/labwc/labwc/pull/2621
[#2627]: https://github.com/labwc/labwc/pull/2627
[#2633]: https://github.com/labwc/labwc/pull/2633
[#2638]: https://github.com/labwc/labwc/pull/2638
[#2645]: https://github.com/labwc/labwc/pull/2645
[#2648]: https://github.com/labwc/labwc/pull/2648
[#2651]: https://github.com/labwc/labwc/pull/2651
[#2652]: https://github.com/labwc/labwc/pull/2652
[#2653]: https://github.com/labwc/labwc/pull/2653
[#2657]: https://github.com/labwc/labwc/pull/2657
[#2667]: https://github.com/labwc/labwc/pull/2667
[#2669]: https://github.com/labwc/labwc/pull/2669
[#2678]: https://github.com/labwc/labwc/pull/2678
[#2686]: https://github.com/labwc/labwc/pull/2686
[#2688]: https://github.com/labwc/labwc/pull/2688
[#2692]: https://github.com/labwc/labwc/pull/2692
[#2693]: https://github.com/labwc/labwc/pull/2693
[#2699]: https://github.com/labwc/labwc/pull/2699
[#2700]: https://github.com/labwc/labwc/pull/2700
[#2710]: https://github.com/labwc/labwc/pull/2710
[#2713]: https://github.com/labwc/labwc/pull/2713
[#2722]: https://github.com/labwc/labwc/pull/2722
[#2723]: https://github.com/labwc/labwc/pull/2723
[#2724]: https://github.com/labwc/labwc/pull/2724
[#2727]: https://github.com/labwc/labwc/pull/2727
[#2728]: https://github.com/labwc/labwc/pull/2728
[#2736]: https://github.com/labwc/labwc/pull/2736
[#2737]: https://github.com/labwc/labwc/pull/2737
[#2740]: https://github.com/labwc/labwc/pull/2740
[#2745]: https://github.com/labwc/labwc/pull/2745
[#2755]: https://github.com/labwc/labwc/pull/2755
[#2767]: https://github.com/labwc/labwc/pull/2767
[#2768]: https://github.com/labwc/labwc/pull/2768
[#2770]: https://github.com/labwc/labwc/pull/2770
[#2774]: https://github.com/labwc/labwc/pull/2774
[#2778]: https://github.com/labwc/labwc/pull/2778
[#2787]: https://github.com/labwc/labwc/pull/2787
[#2788]: https://github.com/labwc/labwc/pull/2788
[#2789]: https://github.com/labwc/labwc/pull/2789
[#2790]: https://github.com/labwc/labwc/pull/2790
[#2795]: https://github.com/labwc/labwc/pull/2795
[#2803]: https://github.com/labwc/labwc/pull/2803
[#2808]: https://github.com/labwc/labwc/pull/2808
[#2811]: https://github.com/labwc/labwc/pull/2811
[#2812]: https://github.com/labwc/labwc/pull/2812
[#2814]: https://github.com/labwc/labwc/pull/2814
[#2819]: https://github.com/labwc/labwc/pull/2819
[#2827]: https://github.com/labwc/labwc/pull/2827
[#2829]: https://github.com/labwc/labwc/pull/2829
[#2831]: https://github.com/labwc/labwc/pull/2831
[#2832]: https://github.com/labwc/labwc/pull/2832
[#2837]: https://github.com/labwc/labwc/pull/2837
[#2839]: https://github.com/labwc/labwc/pull/2839
[#2840]: https://github.com/labwc/labwc/pull/2840
[#2844]: https://github.com/labwc/labwc/pull/2844
[#2846]: https://github.com/labwc/labwc/pull/2846
[#2867]: https://github.com/labwc/labwc/pull/2867
[#2868]: https://github.com/labwc/labwc/pull/2868
[#2873]: https://github.com/labwc/labwc/pull/2873
[#2874]: https://github.com/labwc/labwc/pull/2874
[#2877]: https://github.com/labwc/labwc/pull/2877
[#2883]: https://github.com/labwc/labwc/pull/2883
[#2885]: https://github.com/labwc/labwc/pull/2885
[#2886]: https://github.com/labwc/labwc/pull/2886
[#2887]: https://github.com/labwc/labwc/pull/2887
[#2891]: https://github.com/labwc/labwc/pull/2891
[#2895]: https://github.com/labwc/labwc/pull/2895
[#2909]: https://github.com/labwc/labwc/pull/2909
[#2910]: https://github.com/labwc/labwc/pull/2910
[#2914]: https://github.com/labwc/labwc/pull/2914
[#2933]: https://github.com/labwc/labwc/pull/2933
[#2937]: https://github.com/labwc/labwc/pull/2937
[#2939]: https://github.com/labwc/labwc/pull/2939
[#2942]: https://github.com/labwc/labwc/pull/2942
[#2943]: https://github.com/labwc/labwc/pull/2943
[#2944]: https://github.com/labwc/labwc/pull/2944
[#2948]: https://github.com/labwc/labwc/pull/2948
[#2965]: https://github.com/labwc/labwc/pull/2965
[#2967]: https://github.com/labwc/labwc/pull/2967
[#2970]: https://github.com/labwc/labwc/pull/2970
[#2971]: https://github.com/labwc/labwc/pull/2971
[#2972]: https://github.com/labwc/labwc/pull/2972
[#2976]: https://github.com/labwc/labwc/pull/2976
[#2981]: https://github.com/labwc/labwc/pull/2981
[#2994]: https://github.com/labwc/labwc/pull/2994
[#2995]: https://github.com/labwc/labwc/pull/2995
[#2998]: https://github.com/labwc/labwc/pull/2998
[#3002]: https://github.com/labwc/labwc/pull/3002
[#3004]: https://github.com/labwc/labwc/pull/3004
[#3011]: https://github.com/labwc/labwc/pull/3011
[#3015]: https://github.com/labwc/labwc/pull/3015
[#3020]: https://github.com/labwc/labwc/pull/3020
[#3024]: https://github.com/labwc/labwc/pull/3024
[#3028]: https://github.com/labwc/labwc/pull/3028
[#3033]: https://github.com/labwc/labwc/pull/3033
[#3039]: https://github.com/labwc/labwc/pull/3039
[#3042]: https://github.com/labwc/labwc/pull/3042
[#3043]: https://github.com/labwc/labwc/pull/3043
[#3047]: https://github.com/labwc/labwc/pull/3047
[#3049]: https://github.com/labwc/labwc/pull/3049
[#3081]: https://github.com/labwc/labwc/pull/3081
[#3097]: https://github.com/labwc/labwc/pull/3097
[#3099]: https://github.com/labwc/labwc/pull/3099
[#3118]: https://github.com/labwc/labwc/pull/3118
[#3120]: https://github.com/labwc/labwc/pull/3120
[#3124]: https://github.com/labwc/labwc/pull/3124
[#3126]: https://github.com/labwc/labwc/pull/3126
[#3134]: https://github.com/labwc/labwc/pull/3134
[#3145]: https://github.com/labwc/labwc/pull/3145
[#3146]: https://github.com/labwc/labwc/pull/3146
[#3148]: https://github.com/labwc/labwc/pull/3148
[#3153]: https://github.com/labwc/labwc/pull/3153
[#3157]: https://github.com/labwc/labwc/pull/3157
[#3158]: https://github.com/labwc/labwc/pull/3158
[#3168]: https://github.com/labwc/labwc/pull/3168
[#3175]: https://github.com/labwc/labwc/pull/3175
[#3176]: https://github.com/labwc/labwc/pull/3176
[#3177]: https://github.com/labwc/labwc/pull/3177
[#3179]: https://github.com/labwc/labwc/pull/3179
[#3186]: https://github.com/labwc/labwc/pull/3186
[#3187]: https://github.com/labwc/labwc/pull/3187
[#3199]: https://github.com/labwc/labwc/pull/3199
[#3201]: https://github.com/labwc/labwc/pull/3201
[#3208]: https://github.com/labwc/labwc/pull/3208
