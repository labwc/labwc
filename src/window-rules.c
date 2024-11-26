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
other_instances_exist(struct view *self, struct view_query *query)
{
	struct wl_list *views = &self->server->views;
	struct view *view;

	wl_list_for_each(view, views, link) {
		if (view != self && view_matches_query(view, query)) {
			return true;
		}
	}
	return false;
}

static bool
view_matches_criteria(struct window_rule *rule, struct view *view)
{
	struct view_query query = {
		.identifier = rule->identifier,
		.title = rule->title,
		.window_type = rule->window_type,
		.sandbox_engine = rule->sandbox_engine,
		.sandbox_app_id = rule->sandbox_app_id,
		.maximized = VIEW_AXIS_INVALID,
	};

	if (rule->match_once && other_instances_exist(view, &query)) {
		return false;
	}

	return view_matches_query(view, &query);
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
			actions_run(view, view->server, &rule->actions, NULL);
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
			if (rule->want_absorbed_modifier_release_events
					&& !strcasecmp(property,
					"wantAbsorbedModifierReleaseEvents")) {
				return rule->want_absorbed_modifier_release_events;
			}
		}
	}
	return LAB_PROP_UNSPECIFIED;
}
