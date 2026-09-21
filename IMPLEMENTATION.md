# daemonize implementation

## Summary

Adds an optional `<core><daemonize>` setting that hooks labwc into the systemd
user session. When enabled, labwc writes three user units under
`~/.config/systemd/user/` and the shipped `labwc-session` launcher starts labwc
as a systemd service instead of running it directly. Default is `disabled`, and
disabling removes the units again.

Files changed:

- `src/config/rcxml.c` — parse `<core><daemonize>`, accepting `enabled`,
  `disabled`, `true`, `false`
- `src/config/daemonize.c` — write or remove the three units, create the unit
  directory, remove the stale `[Install]` enable symlink
- `src/main.c` — new `labwc --daemonize-apply` option; drop stale
  `WAYLAND_DISPLAY`/`DISPLAY` when running under systemd
- `data/labwc-session` — stop parsing `rc.xml` in shell, call
  `labwc --daemonize-apply`, scope `reset-failed`, guard the shutdown target
- `docs/labwc.1.scd`, `docs/labwc-config.5.scd` — document the option, the units
  and the launcher
- `docs/session-management` — describe the session lifecycle
- `docs/meson.build` — install `session-management` alongside the other files in
  the doc directory
- `t/daemonize.c`, `t/meson.build` — 12 tests covering enable and disable,
  isolated from the real `$HOME`

Already on `dev` and unchanged by this patch: `data/labwc.desktop` uses
`Exec=labwc-session`.

Notable behavior changes:

- `labwc-session` no longer parses `rc.xml` with `sed`. It delegates to
  `labwc --daemonize-apply`, so launcher and compositor can never disagree
  about the setting, and the units are created on the very login the setting
  was turned on.
- Switching `<daemonize>` in either direction takes effect at the next login.
  No reboot, no manual `systemctl`.
- No `[Install]` section. The units are started explicitly by the launcher. An
  enable symlink left behind by an earlier version of this feature is removed
  on both enable and disable.
- When started by the user manager, labwc drops `WAYLAND_DISPLAY`/`DISPLAY` if
  the socket they name no longer exists, so it does not select the nested
  Wayland or X11 backend and die at login. Remote X11 displays are left alone.

Details below.

## What changed

A new optional `<daemonize>` setting was added to `<core>` in `rc.xml`.
It accepts `enabled`, `disabled`, and their boolean equivalents `true` and
`false`, and defaults to `disabled`. When enabled, labwc creates or refreshes
three user-local systemd units under `~/.config/systemd/user/`:

- `labwc.service`
- `labwc-session.target`
- `labwc-shutdown.target`

When disabled, all three are removed if present. This makes the window
manager participate in the user session lifecycle without shipping a
system-wide enabled unit.

A launcher script, `labwc-session`, is installed alongside the compositor.
The desktop entry uses `Exec=labwc-session` so login managers start the
wrapper rather than the compositor directly.

## rc.xml usage

```xml
<core>
  <daemonize>enabled</daemonize>
</core>
```

- Value is case-insensitive.
- Accepted values: `enabled`, `disabled`, `true`, `false`.
- Default: `disabled`.

## Generated systemd user units

Path when enabled:

    ~/.config/systemd/user/labwc.service
    ~/.config/systemd/user/labwc-session.target
    ~/.config/systemd/user/labwc-shutdown.target

`labwc.service` contents:

    [Unit]
    Description=A Wayland window-stacking compositor
    BindsTo=graphical-session.target
    Before=graphical-session.target
    Wants=graphical-session-pre.target
    After=graphical-session-pre.target

    Wants=xdg-desktop-autostart.target
    Before=xdg-desktop-autostart.target

    [Service]
    Slice=session.slice
    Type=notify
    ExecStart=labwc

`labwc-session.target` contents:

    [Unit]
    Description=labwc session
    Documentation=man:labwc(1) man:systemd.special(7)
    BindsTo=graphical-session.target
    Wants=graphical-session-pre.target
    After=graphical-session-pre.target

`labwc-shutdown.target` contents:

    [Unit]
    Description=labwc Session Shutdown
    Documentation=man:labwc(1) man:systemd.special(7)
    DefaultDependencies=no
    StopWhenUnneeded=yes
    Conflicts=graphical-session.target graphical-session-pre.target
    After=graphical-session.target graphical-session-pre.target

None of these units contain an `[Install]` section. They are started
explicitly by `labwc-session` rather than enabled persistently. An
enablement symlink left behind by older versions
(`graphical-session.target.wants/labwc.service`) is removed on both
enable and disable.

## Applying the setting on a single login

`labwc --daemonize-apply` parses `rc.xml` with the same code that runs the
compositor, writes or removes the three units, and quits without starting
the compositor. `labwc-session` calls it before deciding whether to start
`labwc.service` or run `labwc` directly, so switching `<daemonize>` in
either direction takes effect on the next login without a reboot.

The unit directory (`~/.config/systemd/user/`) is created automatically if
it does not exist yet.

## Stale display variables

The systemd user manager outlives individual sessions and can hold
`WAYLAND_DISPLAY`/`DISPLAY` values pointing at a compositor that no longer
exists. When started as a systemd service and the referenced socket is
gone, labwc removes those variables before selecting a backend, so it does
not fall back to the nested Wayland or X11 backend and die at startup.
`labwc-session` also clears them from the manager environment before
starting the service.

## Session launcher

`labwc-session` is the integration point between the display manager and the
user-local systemd units. It:

- detects whether it is already running under `systemd --user` and, if so,
  execs `labwc` directly
- reexecs through the user's login shell when needed
- detects `systemctl`
- checks whether a labwc session is already active
- resets failed state of `labwc.service` and the autostart units
  (`systemctl --user reset-failed labwc.service 'app-*@autostart.service'`)
- imports the login manager environment
- updates the D-Bus activation environment
- runs `systemctl --user daemon-reload` before first start
- applies the `<daemonize>` setting by calling `labwc --daemonize-apply`,
  which creates or removes the units on the same login the setting changed
- starts `labwc.service` if the unit exists; otherwise falls back to
  `exec labwc` for first-run bootstrap
- waits for the compositor to terminate
- starts `labwc-shutdown.target` with `--job-mode=replace-irreversibly`
  to tear down `graphical-session.target` cleanly, but only while that
  target is still active
- unsets the session environment variables it imported on exit

For debugging, `labwc-session` appends its output to
`~/.local/share/labwc.log`.

## Desktop entry

The shipped desktop file now uses:

    Exec=labwc-session

This keeps the existing login-manager integration path while routing startup
through the session wrapper.

## Shutdown behavior

`session_shutdown()` no longer runs `update_activation_env(false)` during
teardown. That removes a blocking D-Bus/systemd environment-update path from
the SIGTERM-driven backend shutdown sequence, reducing the chance of an
assertion in `wlr_backend_finish()`.

## Readiness notification

When `labwc.service` is started under `systemd --user`, `daemonize_notify_ready()`
sends `READY=1` via `sd_notify(1, "READY=1")` or, when `NOTIFY_FD` is set,
writes directly to that file descriptor. This lets systemd consider the
service started before the window manager begins autostart scripts.

## Tests

`test_daemonize` covers creation and removal of all three user-local units,
including idempotent rewrites, missing directories, readonly parents,
disabled cleanup, and automatic creation of the unit directory. The suite
passes 12/12.

The tests cannot touch the real user configuration: `setup`/`teardown`
point `HOME` at a scratch directory created with `mkdtemp`, and
`t/meson.build` additionally runs the test with `HOME` set to the build
directory, so even a stale test binary cannot delete the units of a
running session.

## Testing a local build

After `meson compile -C build`, install the build so that a login actually
runs it:

1. install `build/labwc` to `/usr/local/bin/labwc`
2. install `data/labwc-session` to `/usr/local/bin/labwc-session`
3. make sure no stale `labwc` binary sits earlier in `PATH` (for example a
   leftover `~/.local/bin/labwc`), otherwise `labwc --daemonize-apply` from
   the launcher resolves to the wrong binary
4. run `labwc --daemonize-apply` to write the units, then
   `systemctl --user daemon-reload`
5. log out and back in through a session whose `Exec` is `labwc-session`

The running session is unaffected; the changes take effect at the next
login.

## Build integration

`docs/meson.build` installs `session-management` into
`$datadir/doc/labwc/`, next to `autostart`, `environment`, `rc.xml` and the
rest, so distributors ship the lifecycle description with the compositor. It is
a plain text file like its neighbours, not a man page, so it is not passed
through scdoc.

## Packaging note for updates

Existing users updating to this patch should:

1. install `labwc-session` to their `bindir`
2. ensure the desktop entry uses `Exec=labwc-session`
3. enable `<daemonize>enabled</daemonize>` in `rc.xml`

If stale `~/.config/systemd/user/labwc*.service` or `labwc*.target` files
exist from an earlier install, they can be removed; labwc recreates them
automatically on the next startup.
