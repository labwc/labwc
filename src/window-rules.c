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
matches_criteria(struct window_rule *rule, struct view *view)
{
	const char *id = view_get_string_prop(view, "app_id");
	const char *title = view_get_string_prop(view, "title");

	if (rule->identifier) {
		if (!id || !match_glob(rule->identifier, id)) {
			return false;
		}
	}
	if (rule->title) {
		if (!title || !match_glob(rule->title, title)) {
			return false;
		}
	}
	if (rule->window_type >= 0) {
		if (!view_contains_window_type(view, rule->window_type)) {
			return false;
		}
	}
	return true;
}

static bool
other_instances_exist(struct window_rule *rule, struct view *self)
{
	struct wl_list *views = &self->server->views;
	struct view *view;

	wl_list_for_each(view, views, link) {
		if (view != self && matches_criteria(rule, view)) {
			return true;
		}
	}
	return false;
}

static bool
view_matches_criteria(struct window_rule *rule, struct view *view)
{
	if (rule->match_once && other_instances_exist(rule, view)) {
		return false;
	}
	return matches_criteria(rule, view);
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
			if (rule->ignore_configure_request
					&& !strcasecmp(property, "ignoreConfigureRequest")) {
				return rule->ignore_configure_request;
			}
			if (rule->fixed_position
					&& !strcasecmp(property, "fixedPosition")) {
				return rule->fixed_position;
			}
		}
	}
	return LAB_PROP_UNSPECIFIED;
}
