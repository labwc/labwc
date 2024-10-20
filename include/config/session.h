/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SESSION_H
#define LABWC_SESSION_H

struct server;

/**
 * session_run_script - run a named session script (or, in merge-config mode,
 * all named session scripts) from the XDG path.
 */
void session_run_script(const char *script);

/**
 * session_environment_init - set environment variables based on <key>=<value>
 * pairs in `${XDG_CONFIG_DIRS:-/etc/xdg}/labwc/environment` with user override
 * in `${XDG_CONFIG_HOME:-$HOME/.config}`
 */
void session_environment_init(void);

/**
 * session_autostart_init - run autostart file as shell script
 * Note: Same as `sh ~/.config/labwc/autostart` (or equivalent XDG config dir)
 */
void session_autostart_init(struct server *server);

/**
 * session_shutdown - run session shutdown file as shell script
 * Note: Same as `sh ~/.config/labwc/shutdown` (or equivalent XDG config dir)
 */
void session_shutdown(struct server *server);

#endif /* LABWC_SESSION_H */
