/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_WINDOW_RULES_H
#define LABWC_WINDOW_RULES_H

#include <stdbool.h>
#include <wayland-util.h>
#include "config/types.h"

enum window_rule_event {
	LAB_WINDOW_RULE_EVENT_ON_FIRST_MAP = 0,
};

enum property {
	LAB_PROP_UNSPECIFIED = 0,
	LAB_PROP_UNSET,
	LAB_PROP_FALSE,
	LAB_PROP_TRUE,
};

/*
 * Taskbar scope for per-window task listing.
 *   - 'UNSPECIFIED'	backward compatible fallback to: monitor
 *   - 'HERE'		THIS monitor & THIS workspace
 *   - 'MONITOR'	THIS monitor & ALL workspaces (default)
 *   - 'WORKSPACE'	ALL monitors & THIS workspace
 *   - 'EVERYWHERE'	ALL monitors & ALL workspaces
 */
enum taskbar_scope {
	LAB_TASKBAR_SCOPE_UNSPECIFIED = 0,
	LAB_TASKBAR_SCOPE_HERE,
	LAB_TASKBAR_SCOPE_MONITOR,
	LAB_TASKBAR_SCOPE_WORKSPACE,
	LAB_TASKBAR_SCOPE_EVERYWHERE,
};

/*
 * 'identifier' represents:
 *   - 'app_id' for native Wayland windows
 *   - 'WM_CLASS' for XWayland clients
 */
struct window_rule {
	char *identifier;
	char *title;
	enum lab_window_type window_type;
	char *sandbox_engine;
	char *sandbox_app_id;
	bool match_once;

	enum window_rule_event event;
	struct wl_list actions;

	enum property server_decoration;
	enum property skip_taskbar;
	enum property skip_window_switcher;
	enum property ignore_focus_request;
	enum property ignore_configure_request;
	enum property fixed_position;
	enum property icon_prefer_client;

	enum taskbar_scope scope_taskbar;

	struct wl_list link; /* struct rcxml.window_rules */
};

struct view;

void window_rules_apply(struct view *view, enum window_rule_event event);
enum property window_rules_get_property(struct view *view, const char *property);
enum taskbar_scope window_rules_get_taskbar_scope(struct view *view, const char *scope);

#endif /* LABWC_WINDOW_RULES_H */
