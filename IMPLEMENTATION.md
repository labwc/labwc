# daemonize implementation

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
explicitly by `labwc-session` rather than enabled persistently.

## Session launcher

`labwc-session` is the integration point between the display manager and the
user-local systemd units. It:

- detects whether it is already running under `systemd --user` and, if so,
  execs `labwc` directly
- reexecs through the user's login shell when needed
- detects `systemctl`
- checks whether a labwc session is already active
- runs `systemctl --user reset-failed`
- imports the login manager environment
- updates the D-Bus activation environment
- runs `systemctl --user daemon-reload` before first start
- starts `labwc.service` if the unit exists; otherwise falls back to
  `exec labwc` for first-run bootstrap
- waits for the compositor to terminate
- starts `labwc-shutdown.target` with `--job-mode=replace-irreversibly`
  to tear down `graphical-session.target` cleanly
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
including idempotent rewrites, missing directories, readonly parents, and
disabled cleanup. The current suite passes 11/11.

## Packaging note for updates

Existing users updating to this patch should:

1. install `labwc-session` to their `bindir`
2. ensure the desktop entry uses `Exec=labwc-session`
3. enable `<daemonize>enabled</daemonize>` in `rc.xml`

If stale `~/.config/systemd/user/labwc*.service` or `labwc*.target` files
exist from an earlier install, they can be removed; labwc recreates them
automatically on the next startup.
