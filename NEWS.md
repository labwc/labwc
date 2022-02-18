# NEWS

This file contains significant user-visible changes for each version.
For full changelog, use `git log`

[We are currently in the process of update the code-base to use the wlroots
scene-graph API](https://github.com/johanmalm/labwc/tree/scene)

Tag 0.5.0 is the last minor release before the move to scene-graph. An
associated v0.5 branch exists for patches and patch-tags if needed. It's
not envisaged that new features will be added on that branch though.

## 0.5.0 (2022-02-18)

This release contains the following two breaking changes:

1. Disabling outputs now causes views to be re-arranged, so in the
   context of idle system power management (for example when using
   swaylock), it is no longer suitable to call wlr-randr {--off,--on}
   to enable/disable outputs.

2. The "Drag" mouse-event and the unmaximize-on-move feature require
   slightly different `<mousebind>` settings to feel natural, so suggest
   updating any local `rc.xml` settings in accordance with
   `docs/rc.xml.all`

As usual, this release contains a bunch of fixes and improvements, of
which the most notable feature-type changes are listed below. A big
thank you to @ARDiDo, @Consolatis and @jlindgren90 for much of the hard
work.

- Render overlay layer popups to support sfwbar (issue #239)
    
- Support HiDPI on-screen-display images for outputs with different scales

- Reload environment variables on SIGHUPi (issue #227)

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

## Previous releases

| Date       | Release notes | wlroots version |
|------------|---------------|-----------------|
| 2022-02-18 | [0.5.0]       | 0.15.1          |
| 2021-12-31 | [0.4.0]       | 0.15.0          |
| 2021-06-28 | [0.3.0]       | 0.14.0          |
| 2021-04-15 | [0.2.0]       | 0.13.0          |
| 2021-03-05 | [0.1.0]       | 0.12.0          |

[0.5.0]: https://github.com/labwc/labwc/releases/tag/0.5.0
[0.4.0]: https://github.com/labwc/labwc/releases/tag/0.4.0
[0.3.0]: https://github.com/labwc/labwc/releases/tag/0.3.0
[0.2.0]: https://github.com/labwc/labwc/releases/tag/0.2.0
[0.1.0]: https://github.com/labwc/labwc/releases/tag/0.1.0

