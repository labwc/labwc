# Contributing

## How to Contribute

## Debugging

There is no one-way-fits-all method for debugging, so you have to use your antennae and do some detective work.

This section contains some approachies which may prove useful.

If the compositor crashes, a good starting point is to produce a backtrace by building with ASAN/UBSAN:

```
meson -Db_sanitzise=address,undefined build/
ninja -C build/
```

Get debug log with `labwc -d`. The log can be directed to a file with `labwc -d 2>log.txt`

To see what is happening on the wayland protocol for a specific client, run it with environment variable `WAYLAND_DEBUG` set to 1, for example `WAYLAND_DEBUG=1 foot`.

To see what the compositor is doing on the protocol run `labwc` nested (i.e. start labwc from a terminal in another instance of labwc or some other compositor) with `WAYLAND_DEBUG=server`. This filters out anything from clients.

For wayland clients, you can get a live view of some useful info using [wlhax](https://git.sr.ht/~kennylevinsen/wlhax).

If you think you've got a damage issue, you can run labwc like this: `WLR_SCENE_DEBUG_DAMAGE=highlight labwc` to get a visual indication of damage regions.

To emulate multiple outputs (even if you only have one physical monitor), run with `WLR_WL_OUTPUTS=2 labwc` are similar. See [wlroots/docs/env_vars.md](https://gitlab.freedesktop.org/wlroots/wlroots/-/blob/master/docs/env_vars.md) for more options.

For some types of bugs, it might be useful to find out which mesa driver (.so) you are using. This can be done with `EGL_LOG_LEVEL=debug labwc 2>&1 | grep MESA-LOADER`

You can also get some useful system info with [drm_info](https://github.com/ascent12/drm_info)

## Packaging

Some distributions carry labwc in their repositories or user repositories.

- @ptrcnull (Alpine)
- @narrat (Arch)
- @jbeich (FreeBSD)
- @adcdam (Slackware)

kindly maintain the packages in their respective distro.

Let's keep them informed of new releases and any changes that relate to packaging.
If you are maintaining a labwc package for another distro feel free to open an issue so we can add you to this list.

## Coding Style

Let's try to stick to sircmpwn's [coding style]. If you're not used to it, you
can use [checkpatch.pl] to run a few simple formatting checks.

## Commit Messages

Write [commit messages] like so, keeping the top line to this sort of syntax:

```
cursor: add special feature
```

And please wrap the main commit message at max 74 characters, otherwise `git log` and similar look so weird.

## Naming Convention

There are three types of coordinate systems: surface, output and layout - for
which the variables (sx, sy), (ox, oy) and (lx, ly) are used respectively in
line with wlroots.
With the introduction of the scene-graph API, some wlroots functions also use
node coordinates (nx, ny) but we prefer (sx, sy) where possible.

## Native Language Support

Translators can add their `MY_LOCALE.po` files to the `po` directory
based on `po/labwc.pot` and issue a pull request.


[coding style]: https://git.sr.ht/~sircmpwn/cstyle
[commit messages]: https://gitlab.freedesktop.org/wlroots/wlroots/-/blob/master/CONTRIBUTING.md#commit-messages 
[checkpatch.pl]: https://github.com/johanmalm/checkpatch.pl
