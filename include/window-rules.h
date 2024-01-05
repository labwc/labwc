/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_WINDOW_RULES_H
#define LABWC_WINDOW_RULES_H

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
 * 'identifier' represents:
 *   - 'app_id' for native Wayland windows
 *   - 'WM_CLASS' for XWayland clients
 */

struct window_rule {
	char *identifier;
	char *title;
	bool match_once;

	enum window_rule_event event;
	struct wl_list actions;

	enum property server_decoration;
	enum property skip_taskbar;
	enum property skip_window_switcher;
	enum property ignore_focus_request;
	enum property fixed_position;

	struct wl_list link; /* struct rcxml.window_rules */

	/* Customisation window title and borders*/
	bool has_custom_border;
	float custom_border_color[4];
};

struct view;

void window_rules_apply(struct view *view, enum window_rule_event event);
enum property window_rules_get_property(struct view *view, const char *property);

/**
 * window_rules_get_custom_border_color - check for presence of custom color in window rules
 * @view: view data
 * @return: pointer to custom color or NULL if there is no custom color
 */
float *window_rules_get_custom_border_color(struct view *view);

#endif /* LABWC_WINDOW_RULES_H */
