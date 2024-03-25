// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdbool.h>
#include <cairo.h>
#include <glib.h>
#include <strings.h>
#include <wlr/util/log.h>
#include "action.h"
#include "common/match.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "view.h"
#include "window-rules.h"

static bool
other_instances_exist(struct view *self, const char *id, const char *title)
{
	struct wl_list *views = &self->server->views;
	const char *prop = NULL;
	struct view *view;

	wl_list_for_each(view, views, link) {
		if (view == self) {
			continue;
		}
		if (id) {
			prop = view_get_string_prop(view, "app_id");
			if (prop && !strcmp(prop, id)) {
				return true;
			}
		}
		if (title) {
			prop = view_get_string_prop(view, "title");
			if (prop && !strcmp(prop, title)) {
				return true;
			}
		}
	}
	return false;
}

/* Try to match against identifier AND title (if set) */
static bool
view_matches_criteria(struct window_rule *rule, struct view *view)
{
	const char *id = view_get_string_prop(view, "app_id");
	const char *title = view_get_string_prop(view, "title");

	if (rule->match_once && other_instances_exist(view, id, title)) {
		return false;
	}

	if (rule->identifier && rule->title) {
		if (!id || !title) {
			return false;
		}
		return match_glob(rule->identifier, id)
			&& match_glob(rule->title, title);
	} else if (rule->identifier) {
		if (!id) {
			return false;
		}
		return match_glob(rule->identifier, id);
	} else if (rule->title) {
		if (!title) {
			return false;
		}
		return match_glob(rule->title, title);
	} else {
		wlr_log(WLR_ERROR, "rule has no identifier or title\n");
		return false;
	}
}

void
window_rules_apply(struct view *view, enum window_rule_event event)
{
	struct window_rule *rule;
	wl_list_for_each(rule, &rc.window_rules, link) {
		if (rule->event != event) {
			continue;
		}
		if (view_matches_criteria(rule, view)) {
			actions_run(view, view->server, &rule->actions, 0);
		}
	}
}

enum property
window_rules_get_property(struct view *view, const char *property)
{
	assert(property);

	/*
	 * We iterate in reverse here because later items in list have higher
	 * priority. For example, in the config below we want the return value
	 * for foot's "serverDecoration" property to be "default".
	 *
	 *     <windowRules>
	 *       <windowRule identifier="*" serverDecoration="no"/>
	 *       <windowRule identifier="foot" serverDecoration="default"/>
	 *     </windowRules>
	 */
	struct window_rule *rule;
	wl_list_for_each_reverse(rule, &rc.window_rules, link) {
		/*
		 * Only return if property != LAB_PROP_UNSPECIFIED otherwise a
		 * <windowRule> which does not set a particular property
		 * attribute would still return here if that property was asked
		 * for.
		 */
		if (view_matches_criteria(rule, view)) {
			if (rule->server_decoration
					&& !strcasecmp(property, "serverDecoration")) {
				return rule->server_decoration;
			}
			if (rule->skip_taskbar
					&& !strcasecmp(property, "skipTaskbar")) {
				return rule->skip_taskbar;
			}
			if (rule->skip_window_switcher
					&& !strcasecmp(property, "skipWindowSwitcher")) {
				return rule->skip_window_switcher;
			}
			if (rule->ignore_focus_request
					&& !strcasecmp(property, "ignoreFocusRequest")) {
				return rule->ignore_focus_request;
			}
			if (rule->fixed_position
					&& !strcasecmp(property, "fixedPosition")) {
				return rule->fixed_position;
			}
		}
	}
	return LAB_PROP_UNSPECIFIED;
}

float*
window_rules_get_custom_border_color(struct view *view)
{
	struct window_rule *rule;
	wl_list_for_each_reverse(rule, &rc.window_rules, link) {
		if (view_matches_criteria(rule, view)) {
			if (rule->has_custom_border) {
				return rule->custom_border_color;
			}
		}
	}
	return NULL;
}
