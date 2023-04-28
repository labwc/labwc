// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include <assert.h>
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

void
window_rules_apply(struct view *view, enum window_rule_event event)
{
	const char *identifier = view_get_string_prop(view, "app_id");
	if (!identifier) {
		return;
	}

	struct window_rule *rule;
	wl_list_for_each(rule, &rc.window_rules, link) {
		if (!rule->identifier || rule->event != event) {
			continue;
		}
		if (match_glob(rule->identifier, identifier)) {
			actions_run(view, view->server, &rule->actions, 0);
		}
	}
}

enum property
window_rules_get_property(struct view *view, const char *property)
{
	assert(property);

	const char *identifier = view_get_string_prop(view, "app_id");
	if (!identifier) {
		goto out;
	}

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
		if (!rule->identifier) {
			continue;
		}
		/*
		 * Only return if property != LAB_PROP_UNSPECIFIED otherwise a
		 * <windowRule> which does not set a particular property
		 * attribute would still return here if that property was asked
		 * for.
		 */
		if (match_glob(rule->identifier, identifier)) {
			if (!strcasecmp(property, "serverDecoration")) {
				if (rule->server_decoration) {
					return rule->server_decoration;
				}
			}
		}
	}
out:
	return LAB_PROP_UNSPECIFIED;
}
