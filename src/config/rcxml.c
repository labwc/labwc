// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <fcntl.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include <wlr/version.h>
#include "action.h"
#include "common/dir.h"
#include "common/list.h"
#include "common/macros.h"
#include "common/mem.h"
#include "common/nodename.h"
#include "common/parse-bool.h"
#include "common/parse-double.h"
#include "common/string-helpers.h"
#include "common/three-state.h"
#include "config/default-bindings.h"
#include "config/keybind.h"
#include "config/libinput.h"
#include "config/mousebind.h"
#include "config/tablet.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "osd.h"
#include "regions.h"
#include "view.h"
#include "window-rules.h"
#include "workspaces.h"

#define LAB_WLR_VERSION_OLDER_THAN(major, minor, micro) \
	(WLR_VERSION_NUM < (((major) << 16) | ((minor) << 8) | (micro)))

struct parser_state {
	bool in_regions;
	bool in_usable_area_override;
	bool in_keybind;
	bool in_mousebind;
	bool in_touch;
	bool in_libinput_category;
	bool in_window_switcher_field;
	bool in_window_rules;
	bool in_action_query;
	bool in_action_then_branch;
	bool in_action_else_branch;
	bool in_action_none_branch;
	struct usable_area_override *current_usable_area_override;
	struct keybind *current_keybind;
	struct mousebind *current_mousebind;
	struct touch_config_entry *current_touch;
	struct libinput_category *current_libinput_category;
	const char *current_mouse_context;
	struct action *current_keybind_action;
	struct action *current_mousebind_action;
	struct region *current_region;
	struct window_switcher_field *current_field;
	struct window_rule *current_window_rule;
	struct action *current_window_rule_action;
	struct view_query *current_view_query;
	struct action *current_child_action;
};

/* for backward compatibility of <mouse><scrollFactor> */
static double mouse_scroll_factor = -1;

enum font_place {
	FONT_PLACE_NONE = 0,
	FONT_PLACE_UNKNOWN,
	FONT_PLACE_ACTIVEWINDOW,
	FONT_PLACE_INACTIVEWINDOW,
	FONT_PLACE_MENUHEADER,
	FONT_PLACE_MENUITEM,
	FONT_PLACE_OSD,
	/* TODO: Add all places based on Openbox's rc.xml */
};

static void load_default_key_bindings(void);
static void load_default_mouse_bindings(void);

static int
parse_window_type(const char *type)
{
	if (!type) {
		return -1;
	}
	if (!strcasecmp(type, "desktop")) {
		return NET_WM_WINDOW_TYPE_DESKTOP;
	} else if (!strcasecmp(type, "dock")) {
		return NET_WM_WINDOW_TYPE_DOCK;
	} else if (!strcasecmp(type, "toolbar")) {
		return NET_WM_WINDOW_TYPE_TOOLBAR;
	} else if (!strcasecmp(type, "menu")) {
		return NET_WM_WINDOW_TYPE_MENU;
	} else if (!strcasecmp(type, "utility")) {
		return NET_WM_WINDOW_TYPE_UTILITY;
	} else if (!strcasecmp(type, "splash")) {
		return NET_WM_WINDOW_TYPE_SPLASH;
	} else if (!strcasecmp(type, "dialog")) {
		return NET_WM_WINDOW_TYPE_DIALOG;
	} else if (!strcasecmp(type, "dropdown_menu")) {
		return NET_WM_WINDOW_TYPE_DROPDOWN_MENU;
	} else if (!strcasecmp(type, "popup_menu")) {
		return NET_WM_WINDOW_TYPE_POPUP_MENU;
	} else if (!strcasecmp(type, "tooltip")) {
		return NET_WM_WINDOW_TYPE_TOOLTIP;
	} else if (!strcasecmp(type, "notification")) {
		return NET_WM_WINDOW_TYPE_NOTIFICATION;
	} else if (!strcasecmp(type, "combo")) {
		return NET_WM_WINDOW_TYPE_COMBO;
	} else if (!strcasecmp(type, "dnd")) {
		return NET_WM_WINDOW_TYPE_DND;
	} else if (!strcasecmp(type, "normal")) {
		return NET_WM_WINDOW_TYPE_NORMAL;
	} else {
		return -1;
	}
}

/*
 * Openbox/labwc comparison
 *
 * Instead of openbox's <titleLayout>WLIMC</title> we use
 *
 *     <titlebar>
 *       <layout>menu:iconfiy,max,close</layout>
 *       <showTitle>yes|no</showTitle>
 *     </titlebar>
 *
 * ...using the icon names (like iconify.xbm) without the file extension for the
 * identifier.
 *
 * labwc        openbox     description
 * -----        -------     -----------
 * menu         W           Open window menu (client-menu)
 * iconfiy      I           Iconify (aka minimize)
 * max          M           Maximize toggle
 * close        C           Close
 * shade        S           Shade toggle
 * desk         D           All-desktops toggle (aka omnipresent)
 */
static void
fill_section(const char *content, struct wl_list *list, uint32_t *found_buttons)
{
	gchar **identifiers = g_strsplit(content, ",", -1);
	for (size_t i = 0; identifiers[i]; ++i) {
		char *identifier = identifiers[i];
		if (string_null_or_empty(identifier)) {
			continue;
		}
		enum ssd_part_type type = LAB_SSD_NONE;
		if (!strcmp(identifier, "icon")) {
#if HAVE_LIBSFDO
			type = LAB_SSD_BUTTON_WINDOW_ICON;
#else
			wlr_log(WLR_ERROR, "libsfdo is not linked. "
				"Replacing 'icon' in titlebar layout with 'menu'.");
			type = LAB_SSD_BUTTON_WINDOW_MENU;
#endif
		} else if (!strcmp(identifier, "menu")) {
			type = LAB_SSD_BUTTON_WINDOW_MENU;
		} else if (!strcmp(identifier, "iconify")) {
			type = LAB_SSD_BUTTON_ICONIFY;
		} else if (!strcmp(identifier, "max")) {
			type = LAB_SSD_BUTTON_MAXIMIZE;
		} else if (!strcmp(identifier, "close")) {
			type = LAB_SSD_BUTTON_CLOSE;
		} else if (!strcmp(identifier, "shade")) {
			type = LAB_SSD_BUTTON_SHADE;
		} else if (!strcmp(identifier, "desk")) {
			type = LAB_SSD_BUTTON_OMNIPRESENT;
		} else {
			wlr_log(WLR_ERROR, "invalid titleLayout identifier '%s'",
				identifier);
			continue;
		}

		assert(type != LAB_SSD_NONE);

		if (*found_buttons & (1 << type)) {
			wlr_log(WLR_ERROR, "ignoring duplicated button type '%s'",
				identifier);
			continue;
		}

		*found_buttons |= (1 << type);

		struct title_button *item = znew(*item);
		item->type = type;
		wl_list_append(list, &item->link);
	}
	g_strfreev(identifiers);
}

static void
fill_title_layout(char *content)
{
	struct wl_list *sections[] = {
		&rc.title_buttons_left,
		&rc.title_buttons_right,
	};

	gchar **parts = g_strsplit(content, ":", -1);

	if (g_strv_length(parts) != 2) {
		wlr_log(WLR_ERROR, "<titlebar><layout> must contain one colon");
		goto err;
	}

	uint32_t found_buttons = 0;
	for (size_t i = 0; parts[i]; ++i) {
		fill_section(parts[i], sections[i], &found_buttons);
	}

	rc.title_layout_loaded = true;
err:
	g_strfreev(parts);
}

static void
fill_usable_area_override(char *nodename, char *content, struct parser_state *state)
{
	if (!strcasecmp(nodename, "margin")) {
		state->current_usable_area_override = znew(*state->current_usable_area_override);
		wl_list_append(&rc.usable_area_overrides,
				&state->current_usable_area_override->link);
		return;
	}
	string_truncate_at_pattern(nodename, ".margin");
	if (!content) {
		/* nop */
	} else if (!state->current_usable_area_override) {
		wlr_log(WLR_ERROR, "no usable-area-override object");
	} else if (!strcmp(nodename, "output")) {
		xstrdup_replace(state->current_usable_area_override->output, content);
	} else if (!strcmp(nodename, "left")) {
		state->current_usable_area_override->margin.left = atoi(content);
	} else if (!strcmp(nodename, "right")) {
		state->current_usable_area_override->margin.right = atoi(content);
	} else if (!strcmp(nodename, "top")) {
		state->current_usable_area_override->margin.top = atoi(content);
	} else if (!strcmp(nodename, "bottom")) {
		state->current_usable_area_override->margin.bottom = atoi(content);
	} else {
		wlr_log(WLR_ERROR, "Unexpected data usable-area-override parser: %s=\"%s\"",
			nodename, content);
	}
}

/* Does a boolean-parse but also allows 'default' */
static void
set_property(const char *str, enum property *variable)
{
	if (!str || !strcasecmp(str, "default")) {
		*variable = LAB_PROP_UNSET;
		return;
	}
	int ret = parse_bool(str, -1);
	if (ret < 0) {
		return;
	}
	*variable = ret ? LAB_PROP_TRUE : LAB_PROP_FALSE;
}

static void
fill_window_rule(char *nodename, char *content, struct parser_state *state)
{
	if (!strcasecmp(nodename, "windowRule.windowRules")) {
		state->current_window_rule = znew(*state->current_window_rule);
		state->current_window_rule->window_type = -1; // Window types are >= 0
		wl_list_append(&rc.window_rules, &state->current_window_rule->link);
		wl_list_init(&state->current_window_rule->actions);
		return;
	}

	string_truncate_at_pattern(nodename, ".windowrule.windowrules");
	if (!content) {
		/* nop */
	} else if (!state->current_window_rule) {
		wlr_log(WLR_ERROR, "no window-rule");

	/* Criteria */
	} else if (!strcmp(nodename, "identifier")) {
		xstrdup_replace(state->current_window_rule->identifier, content);
	} else if (!strcmp(nodename, "title")) {
		xstrdup_replace(state->current_window_rule->title, content);
	} else if (!strcmp(nodename, "type")) {
		state->current_window_rule->window_type = parse_window_type(content);
	} else if (!strcasecmp(nodename, "matchOnce")) {
		set_bool(content, &state->current_window_rule->match_once);
	} else if (!strcasecmp(nodename, "sandboxEngine")) {
		xstrdup_replace(state->current_window_rule->sandbox_engine, content);
	} else if (!strcasecmp(nodename, "sandboxAppId")) {
		xstrdup_replace(state->current_window_rule->sandbox_app_id, content);

	/* Event */
	} else if (!strcmp(nodename, "event")) {
		/*
		 * This is just in readiness for adding any other types of
		 * events in the future. We default to onFirstMap anyway.
		 */
		if (!strcasecmp(content, "onFirstMap")) {
			state->current_window_rule->event = LAB_WINDOW_RULE_EVENT_ON_FIRST_MAP;
		}

	/* Properties */
	} else if (!strcasecmp(nodename, "serverDecoration")) {
		set_property(content, &state->current_window_rule->server_decoration);
	} else if (!strcasecmp(nodename, "skipTaskbar")) {
		set_property(content, &state->current_window_rule->skip_taskbar);
	} else if (!strcasecmp(nodename, "skipWindowSwitcher")) {
		set_property(content, &state->current_window_rule->skip_window_switcher);
	} else if (!strcasecmp(nodename, "ignoreFocusRequest")) {
		set_property(content, &state->current_window_rule->ignore_focus_request);
	} else if (!strcasecmp(nodename, "ignoreConfigureRequest")) {
		set_property(content, &state->current_window_rule->ignore_configure_request);
	} else if (!strcasecmp(nodename, "fixedPosition")) {
		set_property(content, &state->current_window_rule->fixed_position);

	/* Actions */
	} else if (!strcmp(nodename, "name.action")) {
		state->current_window_rule_action = action_create(content);
		if (state->current_window_rule_action) {
			wl_list_append(&state->current_window_rule->actions,
				&state->current_window_rule_action->link);
		}
	} else if (!state->current_window_rule_action) {
		wlr_log(WLR_ERROR, "expect <action name=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
	} else {
		action_arg_from_xml_node(state->current_window_rule_action, nodename, content);
	}
}

static void
fill_window_switcher_field(char *nodename, char *content, struct parser_state *state)
{
	if (!strcasecmp(nodename, "field.fields.windowswitcher")) {
		state->current_field = osd_field_create();
		wl_list_append(&rc.window_switcher.fields, &state->current_field->link);
		return;
	}

	string_truncate_at_pattern(nodename, ".field.fields.windowswitcher");
	if (!content) {
		/* intentionally left empty */
	} else if (!state->current_field) {
		wlr_log(WLR_ERROR, "no <field>");
	} else {
		osd_field_arg_from_xml_node(state->current_field, nodename, content);
	}
}

static void
fill_region(char *nodename, char *content, struct parser_state *state)
{
	string_truncate_at_pattern(nodename, ".region.regions");

	if (!strcasecmp(nodename, "region.regions")) {
		state->current_region = znew(*state->current_region);
		wl_list_append(&rc.regions, &state->current_region->link);
	} else if (!content) {
		/* intentionally left empty */
	} else if (!state->current_region) {
		wlr_log(WLR_ERROR, "Expecting <region name=\"\" before %s='%s'",
			nodename, content);
	} else if (!strcasecmp(nodename, "name")) {
		/* Prevent leaking memory if config contains multiple names */
		if (!state->current_region->name) {
			state->current_region->name = xstrdup(content);
		}
	} else if (strstr("xywidtheight", nodename) && !strchr(content, '%')) {
		wlr_log(WLR_ERROR, "Removing invalid region '%s': %s='%s' misses"
			" a trailing %%", state->current_region->name, nodename, content);
		wl_list_remove(&state->current_region->link);
		zfree(state->current_region->name);
		zfree(state->current_region);
	} else if (!strcmp(nodename, "x")) {
		state->current_region->percentage.x = atoi(content);
	} else if (!strcmp(nodename, "y")) {
		state->current_region->percentage.y = atoi(content);
	} else if (!strcmp(nodename, "width")) {
		state->current_region->percentage.width = atoi(content);
	} else if (!strcmp(nodename, "height")) {
		state->current_region->percentage.height = atoi(content);
	} else {
		wlr_log(WLR_ERROR, "Unexpected data in region parser: %s=\"%s\"",
			nodename, content);
	}
}

static void
fill_action_query(char *nodename, char *content, struct action *action, struct parser_state *state)
{
	if (!action) {
		wlr_log(WLR_ERROR, "No parent action for query: %s=%s", nodename, content);
		return;
	}

	string_truncate_at_pattern(nodename, ".keybind.keyboard");
	string_truncate_at_pattern(nodename, ".mousebind.context.mouse");

	if (!strcasecmp(nodename, "query.action")) {
		state->current_view_query = NULL;
	}

	string_truncate_at_pattern(nodename, ".query.action");

	if (!content) {
		return;
	}

	if (!state->current_view_query) {
		struct wl_list *queries = action_get_querylist(action, "query");
		if (!queries) {
			action_arg_add_querylist(action, "query");
			queries = action_get_querylist(action, "query");
		}
		state->current_view_query = view_query_create();
		wl_list_append(queries, &state->current_view_query->link);
	}

	if (!strcasecmp(nodename, "identifier")) {
		xstrdup_replace(state->current_view_query->identifier, content);
	} else if (!strcasecmp(nodename, "title")) {
		xstrdup_replace(state->current_view_query->title, content);
	} else if (!strcmp(nodename, "type")) {
		state->current_view_query->window_type = parse_window_type(content);
	} else if (!strcasecmp(nodename, "sandboxEngine")) {
		xstrdup_replace(state->current_view_query->sandbox_engine, content);
	} else if (!strcasecmp(nodename, "sandboxAppId")) {
		xstrdup_replace(state->current_view_query->sandbox_app_id, content);
	} else if (!strcasecmp(nodename, "shaded")) {
		state->current_view_query->shaded = parse_three_state(content);
	} else if (!strcasecmp(nodename, "maximized")) {
		state->current_view_query->maximized = view_axis_parse(content);
	} else if (!strcasecmp(nodename, "iconified")) {
		state->current_view_query->iconified = parse_three_state(content);
	} else if (!strcasecmp(nodename, "focused")) {
		state->current_view_query->focused = parse_three_state(content);
	} else if (!strcasecmp(nodename, "omnipresent")) {
		state->current_view_query->omnipresent = parse_three_state(content);
	} else if (!strcasecmp(nodename, "tiled")) {
		state->current_view_query->tiled = view_edge_parse(content);
	} else if (!strcasecmp(nodename, "tiled_region")) {
		xstrdup_replace(state->current_view_query->tiled_region, content);
	} else if (!strcasecmp(nodename, "desktop")) {
		xstrdup_replace(state->current_view_query->desktop, content);
	} else if (!strcasecmp(nodename, "decoration")) {
		state->current_view_query->decoration = ssd_mode_parse(content);
	} else if (!strcasecmp(nodename, "monitor")) {
		xstrdup_replace(state->current_view_query->monitor, content);
	}
}

static void
fill_child_action(char *nodename, char *content, struct action *parent,
	const char *branch_name, struct parser_state *state)
{
	if (!parent) {
		wlr_log(WLR_ERROR, "No parent action for branch: %s=%s", nodename, content);
		return;
	}

	string_truncate_at_pattern(nodename, ".keybind.keyboard");
	string_truncate_at_pattern(nodename, ".mousebind.context.mouse");
	string_truncate_at_pattern(nodename, ".then.action");
	string_truncate_at_pattern(nodename, ".else.action");
	string_truncate_at_pattern(nodename, ".none.action");

	if (!strcasecmp(nodename, "action")) {
		state->current_child_action = NULL;
	}

	if (!content) {
		return;
	}

	struct wl_list *siblings = action_get_actionlist(parent, branch_name);
	if (!siblings) {
		action_arg_add_actionlist(parent, branch_name);
		siblings = action_get_actionlist(parent, branch_name);
	}

	if (!strcasecmp(nodename, "name.action")) {
		if (!strcasecmp(content, "If") || !strcasecmp(content, "ForEach")) {
			wlr_log(WLR_ERROR, "action '%s' cannot be a child action", content);
			return;
		}
		state->current_child_action = action_create(content);
		if (state->current_child_action) {
			wl_list_append(siblings, &state->current_child_action->link);
		}
	} else if (!state->current_child_action) {
		wlr_log(WLR_ERROR, "expect <action name=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
	} else {
		action_arg_from_xml_node(state->current_child_action, nodename, content);
	}
}

static void
fill_keybind(char *nodename, char *content, struct parser_state *state)
{
	if (!content) {
		return;
	}
	string_truncate_at_pattern(nodename, ".keybind.keyboard");
	if (!strcmp(nodename, "key")) {
		state->current_keybind = keybind_create(content);
		state->current_keybind_action = NULL;
		/*
		 * If an invalid keybind has been provided,
		 * keybind_create() complains.
		 */
		if (!state->current_keybind) {
			wlr_log(WLR_ERROR, "Invalid keybind: %s", content);
			return;
		}
	} else if (!state->current_keybind) {
		wlr_log(WLR_ERROR, "expect <keybind key=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
	} else if (!strcasecmp(nodename, "onRelease")) {
		set_bool(content, &state->current_keybind->on_release);
	} else if (!strcasecmp(nodename, "layoutDependent")) {
		set_bool(content, &state->current_keybind->use_syms_only);
	} else if (!strcasecmp(nodename, "allowWhenLocked")) {
		set_bool(content, &state->current_keybind->allow_when_locked);
	} else if (!strcmp(nodename, "name.action")) {
		state->current_keybind_action = action_create(content);
		if (state->current_keybind_action) {
			wl_list_append(&state->current_keybind->actions,
				&state->current_keybind_action->link);
		}
	} else if (!state->current_keybind_action) {
		wlr_log(WLR_ERROR, "expect <action name=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
	} else {
		/*
		 * Here we deal with action sub-elements such as <to>, <output>,
		 * <region>, <direction> and so on. This is common to key- and
		 * mousebinds.
		 */
		action_arg_from_xml_node(state->current_keybind_action, nodename, content);
	}
}

static void
fill_mousebind(char *nodename, char *content, struct parser_state *state)
{
	/*
	 * Example of what we are parsing:
	 * <mousebind button="Left" action="DoubleClick">
	 *   <action name="Focus"/>
	 *   <action name="Raise"/>
	 *   <action name="ToggleMaximize"/>
	 * </mousebind>
	 */

	if (!state->current_mouse_context) {
		wlr_log(WLR_ERROR, "expect <context name=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
		return;
	} else if (!strcmp(nodename, "mousebind.context.mouse")) {
		wlr_log(WLR_INFO, "create mousebind for %s",
			state->current_mouse_context);
		state->current_mousebind = mousebind_create(state->current_mouse_context);
		state->current_mousebind_action = NULL;
		return;
	} else if (!content) {
		return;
	}

	string_truncate_at_pattern(nodename, ".mousebind.context.mouse");
	if (!state->current_mousebind) {
		wlr_log(WLR_ERROR,
			"expect <mousebind button=\"\" action=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
	} else if (!strcmp(nodename, "button")) {
		state->current_mousebind->button = mousebind_button_from_str(content,
			&state->current_mousebind->modifiers);
	} else if (!strcmp(nodename, "direction")) {
		state->current_mousebind->direction = mousebind_direction_from_str(content,
			&state->current_mousebind->modifiers);
	} else if (!strcmp(nodename, "action")) {
		/* <mousebind button="" action="EVENT"> */
		state->current_mousebind->mouse_event =
			mousebind_event_from_str(content);
	} else if (!strcmp(nodename, "name.action")) {
		state->current_mousebind_action = action_create(content);
		if (state->current_mousebind_action) {
			wl_list_append(&state->current_mousebind->actions,
				&state->current_mousebind_action->link);
		}
	} else if (!state->current_mousebind_action) {
		wlr_log(WLR_ERROR, "expect <action name=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
	} else {
		action_arg_from_xml_node(state->current_mousebind_action, nodename, content);
	}
}

static void
fill_touch(char *nodename, char *content, struct parser_state *state)
{
	if (!strcasecmp(nodename, "touch")) {
		state->current_touch = znew(*state->current_touch);
		wl_list_append(&rc.touch_configs, &state->current_touch->link);
		return;
	}

	if (!content) {
		return;
	}

	if (!strcasecmp(nodename, "deviceName.touch")) {
		xstrdup_replace(state->current_touch->device_name, content);
	} else if (!strcasecmp(nodename, "mapToOutput.touch")) {
		xstrdup_replace(state->current_touch->output_name, content);
	} else if (!strcasecmp(nodename, "mouseEmulation.touch")) {
		set_bool(content, &state->current_touch->force_mouse_emulation);
	} else {
		wlr_log(WLR_ERROR, "Unexpected data in touch parser: %s=\"%s\"",
			nodename, content);
	}
}

static int
get_accel_profile(const char *s)
{
	if (!s) {
		return -1;
	}
	if (!strcasecmp(s, "flat")) {
		return LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
	}
	if (!strcasecmp(s, "adaptive")) {
		return LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
	}
	return -1;
}

static int
get_send_events_mode(const char *s)
{
	if (!s) {
		goto err;
	}

	int ret = parse_bool(s, -1);
	if (ret >= 0) {
		return ret
			? LIBINPUT_CONFIG_SEND_EVENTS_ENABLED
			: LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
	}

	if (!strcasecmp(s, "disabledOnExternalMouse")) {
		return LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
	}

err:
	wlr_log(WLR_INFO, "Not a recognised send events mode");
	return -1;
}

static void
fill_libinput_category(char *nodename, char *content, struct parser_state *state)
{
	/*
	 * Create a new profile (libinput-category) on `<libinput><device>`
	 * so that the 'default' profile can be created without even providing a
	 * category="" attribute (same as <device category="default">...)
	 */
	if (!strcmp(nodename, "device.libinput")) {
		state->current_libinput_category = libinput_category_create();
	}

	if (!content) {
		return;
	}

	if (!state->current_libinput_category) {
		return;
	}

	string_truncate_at_pattern(nodename, ".device.libinput");

	if (!strcmp(nodename, "category")) {
		/*
		 * First we try to get a type based on a number of pre-defined
		 * terms, for example: 'default', 'touch', 'touchpad' and
		 * 'non-touch'
		 */
		state->current_libinput_category->type = get_device_type(content);

		/*
		 * If we couldn't match against any of those terms, we use the
		 * provided value to define the device name that the settings
		 * should be applicable to.
		 */
		if (state->current_libinput_category->type == LAB_LIBINPUT_DEVICE_NONE) {
			xstrdup_replace(state->current_libinput_category->name, content);
		}
	} else if (!strcasecmp(nodename, "naturalScroll")) {
		set_bool_as_int(content, &state->current_libinput_category->natural_scroll);
	} else if (!strcasecmp(nodename, "leftHanded")) {
		set_bool_as_int(content, &state->current_libinput_category->left_handed);
	} else if (!strcasecmp(nodename, "pointerSpeed")) {
		set_float(content, &state->current_libinput_category->pointer_speed);
		if (state->current_libinput_category->pointer_speed < -1) {
			state->current_libinput_category->pointer_speed = -1;
		} else if (state->current_libinput_category->pointer_speed > 1) {
			state->current_libinput_category->pointer_speed = 1;
		}
	} else if (!strcasecmp(nodename, "tap")) {
		int ret = parse_bool(content, -1);
		if (ret < 0) {
			return;
		}
		state->current_libinput_category->tap = ret
			? LIBINPUT_CONFIG_TAP_ENABLED
			: LIBINPUT_CONFIG_TAP_DISABLED;
	} else if (!strcasecmp(nodename, "tapButtonMap")) {
		if (!strcmp(content, "lrm")) {
			state->current_libinput_category->tap_button_map =
				LIBINPUT_CONFIG_TAP_MAP_LRM;
		} else if (!strcmp(content, "lmr")) {
			state->current_libinput_category->tap_button_map =
				LIBINPUT_CONFIG_TAP_MAP_LMR;
		} else {
			wlr_log(WLR_ERROR, "invalid tapButtonMap");
		}
	} else if (!strcasecmp(nodename, "tapAndDrag")) {
		int ret = parse_bool(content, -1);
		if (ret < 0) {
			return;
		}
		state->current_libinput_category->tap_and_drag = ret
			? LIBINPUT_CONFIG_DRAG_ENABLED
			: LIBINPUT_CONFIG_DRAG_DISABLED;
	} else if (!strcasecmp(nodename, "dragLock")) {
		int ret = parse_bool(content, -1);
		if (ret < 0) {
			return;
		}
		state->current_libinput_category->drag_lock = ret
			? LIBINPUT_CONFIG_DRAG_LOCK_ENABLED
			: LIBINPUT_CONFIG_DRAG_LOCK_DISABLED;
	} else if (!strcasecmp(nodename, "accelProfile")) {
		state->current_libinput_category->accel_profile =
			get_accel_profile(content);
	} else if (!strcasecmp(nodename, "middleEmulation")) {
		int ret = parse_bool(content, -1);
		if (ret < 0) {
			return;
		}
		state->current_libinput_category->middle_emu = ret
			? LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED
			: LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED;
	} else if (!strcasecmp(nodename, "disableWhileTyping")) {
		int ret = parse_bool(content, -1);
		if (ret < 0) {
			return;
		}
		state->current_libinput_category->dwt = ret
			? LIBINPUT_CONFIG_DWT_ENABLED
			: LIBINPUT_CONFIG_DWT_DISABLED;
	} else if (!strcasecmp(nodename, "clickMethod")) {
		if (!strcasecmp(content, "none")) {
			state->current_libinput_category->click_method =
				LIBINPUT_CONFIG_CLICK_METHOD_NONE;
		} else if (!strcasecmp(content, "clickfinger")) {
			state->current_libinput_category->click_method =
				LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;
		} else if (!strcasecmp(content, "buttonAreas")) {
			state->current_libinput_category->click_method =
				LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
		} else {
			wlr_log(WLR_ERROR, "invalid clickMethod");
		}
	} else if (!strcasecmp(nodename, "sendEventsMode")) {
		state->current_libinput_category->send_events_mode =
			get_send_events_mode(content);
	} else if (!strcasecmp(nodename, "calibrationMatrix")) {
		errno = 0;
		state->current_libinput_category->have_calibration_matrix = true;
		float *mat = state->current_libinput_category->calibration_matrix;
		gchar **elements = g_strsplit(content, " ", -1);
		guint i = 0;
		for (; elements[i]; ++i) {
			char *end_str = NULL;
			mat[i] = strtof(elements[i], &end_str);
			if (errno == ERANGE || *end_str != '\0' || i == 6 || *elements[i] == '\0') {
				wlr_log(WLR_ERROR, "invalid calibration matrix element"
									" %s (index %d), expect six floats",
									elements[i], i);
				state->current_libinput_category->have_calibration_matrix = false;
				errno = 0;
				break;
			}
		}
		if (i != 6 && state->current_libinput_category->have_calibration_matrix) {
			wlr_log(WLR_ERROR, "wrong number of calibration matrix elements,"
								" expected 6, got %d", i);
			state->current_libinput_category->have_calibration_matrix = false;
		}
		g_strfreev(elements);
	} else if (!strcasecmp(nodename, "scrollFactor")) {
		set_double(content, &state->current_libinput_category->scroll_factor);
	}
}

static void
set_font_attr(struct font *font, const char *nodename, const char *content)
{
	if (!strcmp(nodename, "name")) {
		xstrdup_replace(font->name, content);
	} else if (!strcmp(nodename, "size")) {
		font->size = atoi(content);
	} else if (!strcmp(nodename, "slant")) {
		if (!strcasecmp(content, "italic")) {
			font->slant = FONT_SLANT_ITALIC;
		} else if (!strcasecmp(content, "oblique")) {
			font->slant = FONT_SLANT_OBLIQUE;
		} else {
			font->slant = FONT_SLANT_NORMAL;
		}
	} else if (!strcmp(nodename, "weight")) {
		font->weight = !strcasecmp(content, "bold") ?
			FONT_WEIGHT_BOLD : FONT_WEIGHT_NORMAL;
	}
}

static void
fill_font(char *nodename, char *content, enum font_place place)
{
	if (!content) {
		return;
	}
	string_truncate_at_pattern(nodename, ".font.theme");

	switch (place) {
	case FONT_PLACE_NONE:
		/*
		 * If <theme><font></font></theme> is used without a place=""
		 * attribute, we set all font variables
		 */
		set_font_attr(&rc.font_activewindow, nodename, content);
		set_font_attr(&rc.font_inactivewindow, nodename, content);
		set_font_attr(&rc.font_menuheader, nodename, content);
		set_font_attr(&rc.font_menuitem, nodename, content);
		set_font_attr(&rc.font_osd, nodename, content);
		break;
	case FONT_PLACE_ACTIVEWINDOW:
		set_font_attr(&rc.font_activewindow, nodename, content);
		break;
	case FONT_PLACE_INACTIVEWINDOW:
		set_font_attr(&rc.font_inactivewindow, nodename, content);
		break;
	case FONT_PLACE_MENUHEADER:
		set_font_attr(&rc.font_menuheader, nodename, content);
		break;
	case FONT_PLACE_MENUITEM:
		set_font_attr(&rc.font_menuitem, nodename, content);
		break;
	case FONT_PLACE_OSD:
		set_font_attr(&rc.font_osd, nodename, content);
		break;

		/* TODO: implement for all font places */

	default:
		break;
	}
}

static enum font_place
enum_font_place(const char *place)
{
	if (!place || place[0] == '\0') {
		return FONT_PLACE_NONE;
	}
	if (!strcasecmp(place, "ActiveWindow")) {
		return FONT_PLACE_ACTIVEWINDOW;
	} else if (!strcasecmp(place, "InactiveWindow")) {
		return FONT_PLACE_INACTIVEWINDOW;
	} else if (!strcasecmp(place, "MenuHeader")) {
		return FONT_PLACE_MENUHEADER;
	} else if (!strcasecmp(place, "MenuItem")) {
		return FONT_PLACE_MENUITEM;
	} else if (!strcasecmp(place, "OnScreenDisplay")
			|| !strcasecmp(place, "OSD")) {
		return FONT_PLACE_OSD;
	}
	return FONT_PLACE_UNKNOWN;
}

static void
set_adaptive_sync_mode(const char *str, enum adaptive_sync_mode *variable)
{
	if (!strcasecmp(str, "fullscreen")) {
		*variable = LAB_ADAPTIVE_SYNC_FULLSCREEN;
	} else {
		int ret = parse_bool(str, -1);
		if (ret == 1) {
			*variable = LAB_ADAPTIVE_SYNC_ENABLED;
		} else {
			*variable = LAB_ADAPTIVE_SYNC_DISABLED;
		}
	}
}

static void
set_tearing_mode(const char *str, enum tearing_mode *variable)
{
	if (!strcasecmp(str, "fullscreen")) {
		*variable = LAB_TEARING_FULLSCREEN;
	} else if (!strcasecmp(str, "fullscreenForced")) {
		*variable = LAB_TEARING_FULLSCREEN_FORCED;
	} else if (parse_bool(str, -1) == 1) {
		*variable = LAB_TEARING_ENABLED;
	} else {
		*variable = LAB_TEARING_DISABLED;
	}
}

static void
entry(xmlNode *node, char *nodename, char *content, struct parser_state *state)
{
	/* current <theme><font place=""></font></theme> */
	static enum font_place font_place = FONT_PLACE_NONE;

	static uint32_t button_map_from;

	if (!nodename) {
		return;
	}
	string_truncate_at_pattern(nodename, ".openbox_config");
	string_truncate_at_pattern(nodename, ".labwc_config");

	if (getenv("LABWC_DEBUG_CONFIG_NODENAMES")) {
		printf("%s: %s\n", nodename, content);
	}

	if (state->in_usable_area_override) {
		fill_usable_area_override(nodename, content, state);
	}
	if (state->in_keybind) {
		if (state->in_action_query) {
			fill_action_query(nodename, content,
				state->current_keybind_action, state);
		} else if (state->in_action_then_branch) {
			fill_child_action(nodename, content,
				state->current_keybind_action, "then", state);
		} else if (state->in_action_else_branch) {
			fill_child_action(nodename, content,
				state->current_keybind_action, "else", state);
		} else if (state->in_action_none_branch) {
			fill_child_action(nodename, content,
				state->current_keybind_action, "none", state);
		} else {
			fill_keybind(nodename, content, state);
		}
	}
	if (state->in_mousebind) {
		if (state->in_action_query) {
			fill_action_query(nodename, content,
				state->current_mousebind_action, state);
		} else if (state->in_action_then_branch) {
			fill_child_action(nodename, content,
				state->current_mousebind_action, "then", state);
		} else if (state->in_action_else_branch) {
			fill_child_action(nodename, content,
				state->current_mousebind_action, "else", state);
		} else if (state->in_action_none_branch) {
			fill_child_action(nodename, content,
				state->current_mousebind_action, "none", state);
		} else {
			fill_mousebind(nodename, content, state);
		}
	}
	if (state->in_touch) {
		fill_touch(nodename, content, state);
		return;
	}
	if (state->in_libinput_category) {
		fill_libinput_category(nodename, content, state);
		return;
	}
	if (state->in_regions) {
		fill_region(nodename, content, state);
		return;
	}
	if (state->in_window_switcher_field) {
		fill_window_switcher_field(nodename, content, state);
		return;
	}
	if (state->in_window_rules) {
		fill_window_rule(nodename, content, state);
		return;
	}

	/* handle nodes without content, e.g. <keyboard><default /> */
	if (!strcmp(nodename, "default.keyboard")) {
		load_default_key_bindings();
		return;
	}
	if (!strcmp(nodename, "devault.mouse")
			|| !strcmp(nodename, "default.mouse")) {
		load_default_mouse_bindings();
		return;
	}

	if (!strcasecmp(nodename, "map.tablet")) {
		button_map_from = UINT32_MAX;
		return;
	}

	/* handle the rest */
	if (!content) {
		return;
	}
	if (!strcmp(nodename, "place.font.theme")) {
		font_place = enum_font_place(content);
		if (font_place == FONT_PLACE_UNKNOWN) {
			wlr_log(WLR_ERROR, "invalid font place %s", content);
		}
	}

	if (!strcmp(nodename, "decoration.core")) {
		if (!strcmp(content, "client")) {
			rc.xdg_shell_server_side_deco = false;
		} else {
			rc.xdg_shell_server_side_deco = true;
		}
	} else if (!strcmp(nodename, "gap.core")) {
		rc.gap = atoi(content);
	} else if (!strcasecmp(nodename, "adaptiveSync.core")) {
		set_adaptive_sync_mode(content, &rc.adaptive_sync);
	} else if (!strcasecmp(nodename, "allowTearing.core")) {
		set_tearing_mode(content, &rc.allow_tearing);
	} else if (!strcasecmp(nodename, "autoEnableOutputs.core")) {
		set_bool(content, &rc.auto_enable_outputs);
	} else if (!strcasecmp(nodename, "reuseOutputMode.core")) {
		set_bool(content, &rc.reuse_output_mode);
	} else if (!strcmp(nodename, "policy.placement")) {
		enum view_placement_policy policy = view_placement_parse(content);
		if (policy != LAB_PLACE_INVALID) {
			rc.placement_policy = policy;
		}
	} else if (!strcasecmp(nodename, "xwaylandPersistence.core")) {
		set_bool(content, &rc.xwayland_persistence);

#if LAB_WLR_VERSION_OLDER_THAN(0, 18, 2)
		if (!rc.xwayland_persistence) {
			wlr_log(WLR_ERROR, "to avoid the risk of a fatal crash, "
				"setting xwaylandPersistence to 'no' is only "
				"recommended when labwc is compiled against "
				"wlroots >= 0.18.2. See #2371 for details.");
		}
#endif
	} else if (!strcasecmp(nodename, "x.cascadeOffset.placement")) {
		rc.placement_cascade_offset_x = atoi(content);
	} else if (!strcasecmp(nodename, "y.cascadeOffset.placement")) {
		rc.placement_cascade_offset_y = atoi(content);
	} else if (!strcmp(nodename, "name.theme")) {
		xstrdup_replace(rc.theme_name, content);
	} else if (!strcmp(nodename, "icon.theme")) {
		xstrdup_replace(rc.icon_theme_name, content);
	} else if (!strcasecmp(nodename, "fallbackAppIcon.theme")) {
		xstrdup_replace(rc.fallback_app_icon_name, content);
	} else if (!strcasecmp(nodename, "layout.titlebar.theme")) {
		fill_title_layout(content);
	} else if (!strcasecmp(nodename, "showTitle.titlebar.theme")) {
		rc.show_title = parse_bool(content, true);
	} else if (!strcmp(nodename, "cornerradius.theme")) {
		rc.corner_radius = atoi(content);
	} else if (!strcasecmp(nodename, "keepBorder.theme")) {
		set_bool(content, &rc.ssd_keep_border);
	} else if (!strcasecmp(nodename, "dropShadows.theme")) {
		set_bool(content, &rc.shadows_enabled);
	} else if (!strcmp(nodename, "name.font.theme")) {
		fill_font(nodename, content, font_place);
	} else if (!strcmp(nodename, "size.font.theme")) {
		fill_font(nodename, content, font_place);
	} else if (!strcmp(nodename, "slant.font.theme")) {
		fill_font(nodename, content, font_place);
	} else if (!strcmp(nodename, "weight.font.theme")) {
		fill_font(nodename, content, font_place);
	} else if (!strcasecmp(nodename, "followMouse.focus")) {
		set_bool(content, &rc.focus_follow_mouse);
	} else if (!strcasecmp(nodename, "followMouseRequiresMovement.focus")) {
		set_bool(content, &rc.focus_follow_mouse_requires_movement);
	} else if (!strcasecmp(nodename, "raiseOnFocus.focus")) {
		set_bool(content, &rc.raise_on_focus);
	} else if (!strcasecmp(nodename, "doubleClickTime.mouse")) {
		long doubleclick_time_parsed = strtol(content, NULL, 10);
		if (doubleclick_time_parsed > 0) {
			rc.doubleclick_time = doubleclick_time_parsed;
		} else {
			wlr_log(WLR_ERROR, "invalid doubleClickTime");
		}
	} else if (!strcasecmp(nodename, "scrollFactor.mouse")) {
		/* This is deprecated. Show an error message in post_processing() */
		set_double(content, &mouse_scroll_factor);
	} else if (!strcasecmp(nodename, "name.context.mouse")) {
		state->current_mouse_context = content;
		state->current_mousebind = NULL;

	} else if (!strcasecmp(nodename, "repeatRate.keyboard")) {
		rc.repeat_rate = atoi(content);
	} else if (!strcasecmp(nodename, "repeatDelay.keyboard")) {
		rc.repeat_delay = atoi(content);
	} else if (!strcasecmp(nodename, "numlock.keyboard")) {
		bool value;
		set_bool(content, &value);
		rc.kb_numlock_enable = value ? LAB_STATE_ENABLED
			: LAB_STATE_DISABLED;
	} else if (!strcasecmp(nodename, "layoutScope.keyboard")) {
		/*
		 * This can be changed to an enum later on
		 * if we decide to also support "application".
		 */
		rc.kb_layout_per_window = !strcasecmp(content, "window");
	} else if (!strcasecmp(nodename, "screenEdgeStrength.resistance")) {
		rc.screen_edge_strength = atoi(content);
	} else if (!strcasecmp(nodename, "windowEdgeStrength.resistance")) {
		rc.window_edge_strength = atoi(content);
	} else if (!strcasecmp(nodename, "unSnapThreshold.resistance")) {
		rc.unsnap_threshold = atoi(content);
	} else if (!strcasecmp(nodename, "unMaximizeThreshold.resistance")) {
		rc.unmaximize_threshold = atoi(content);
	} else if (!strcasecmp(nodename, "range.snapping")) {
		rc.snap_edge_range = atoi(content);
	} else if (!strcasecmp(nodename, "enabled.overlay.snapping")) {
		set_bool(content, &rc.snap_overlay_enabled);
	} else if (!strcasecmp(nodename, "inner.delay.overlay.snapping")) {
		rc.snap_overlay_delay_inner = atoi(content);
	} else if (!strcasecmp(nodename, "outer.delay.overlay.snapping")) {
		rc.snap_overlay_delay_outer = atoi(content);
	} else if (!strcasecmp(nodename, "topMaximize.snapping")) {
		set_bool(content, &rc.snap_top_maximize);
	} else if (!strcasecmp(nodename, "notifyClient.snapping")) {
		if (!strcasecmp(content, "always")) {
			rc.snap_tiling_events_mode = LAB_TILING_EVENTS_ALWAYS;
		} else if (!strcasecmp(content, "region")) {
			rc.snap_tiling_events_mode = LAB_TILING_EVENTS_REGION;
		} else if (!strcasecmp(content, "edge")) {
			rc.snap_tiling_events_mode = LAB_TILING_EVENTS_EDGE;
		} else if (!strcasecmp(content, "never")) {
			rc.snap_tiling_events_mode = LAB_TILING_EVENTS_NEVER;
		} else {
			wlr_log(WLR_ERROR, "ignoring invalid value for notifyClient");
		}

	/* <windowSwitcher show="" preview="" outlines="" /> */
	} else if (!strcasecmp(nodename, "show.windowSwitcher")) {
		set_bool(content, &rc.window_switcher.show);
	} else if (!strcasecmp(nodename, "preview.windowSwitcher")) {
		set_bool(content, &rc.window_switcher.preview);
	} else if (!strcasecmp(nodename, "outlines.windowSwitcher")) {
		set_bool(content, &rc.window_switcher.outlines);
	} else if (!strcasecmp(nodename, "allWorkspaces.windowSwitcher")) {
		if (parse_bool(content, -1) == true) {
			rc.window_switcher.criteria &=
				~LAB_VIEW_CRITERIA_CURRENT_WORKSPACE;
		}

	/* Remove this long term - just a friendly warning for now */
	} else if (strstr(nodename, "windowswitcher.core")) {
		wlr_log(WLR_ERROR, "<windowSwitcher> should not be child of <core>");

	/* The following three are for backward compatibility only */
	} else if (!strcasecmp(nodename, "show.windowSwitcher.core")) {
		set_bool(content, &rc.window_switcher.show);
	} else if (!strcasecmp(nodename, "preview.windowSwitcher.core")) {
		set_bool(content, &rc.window_switcher.preview);
	} else if (!strcasecmp(nodename, "outlines.windowSwitcher.core")) {
		set_bool(content, &rc.window_switcher.outlines);

	/* The following three are for backward compatibility only */
	} else if (!strcasecmp(nodename, "cycleViewOSD.core")) {
		set_bool(content, &rc.window_switcher.show);
		wlr_log(WLR_ERROR, "<cycleViewOSD> is deprecated."
			" Use <windowSwitcher show=\"\" />");
	} else if (!strcasecmp(nodename, "cycleViewPreview.core")) {
		set_bool(content, &rc.window_switcher.preview);
		wlr_log(WLR_ERROR, "<cycleViewPreview> is deprecated."
			" Use <windowSwitcher preview=\"\" />");
	} else if (!strcasecmp(nodename, "cycleViewOutlines.core")) {
		set_bool(content, &rc.window_switcher.outlines);
		wlr_log(WLR_ERROR, "<cycleViewOutlines> is deprecated."
			" Use <windowSwitcher outlines=\"\" />");

	} else if (!strcasecmp(nodename, "name.names.desktops")) {
		struct workspace *workspace = znew(*workspace);
		workspace->name = xstrdup(content);
		wl_list_append(&rc.workspace_config.workspaces, &workspace->link);
	} else if (!strcasecmp(nodename, "popupTime.desktops")) {
		rc.workspace_config.popuptime = atoi(content);
	} else if (!strcasecmp(nodename, "number.desktops")) {
		rc.workspace_config.min_nr_workspaces = MAX(1, atoi(content));
	} else if (!strcasecmp(nodename, "prefix.desktops")) {
		xstrdup_replace(rc.workspace_config.prefix, content);
	} else if (!strcasecmp(nodename, "popupShow.resize")) {
		if (!strcasecmp(content, "Always")) {
			rc.resize_indicator = LAB_RESIZE_INDICATOR_ALWAYS;
		} else if (!strcasecmp(content, "Never")) {
			rc.resize_indicator = LAB_RESIZE_INDICATOR_NEVER;
		} else if (!strcasecmp(content, "Nonpixel")) {
			rc.resize_indicator = LAB_RESIZE_INDICATOR_NON_PIXEL;
		} else {
			wlr_log(WLR_ERROR, "Invalid value for <resize popupShow />");
		}
	} else if (!strcasecmp(nodename, "drawContents.resize")) {
		set_bool(content, &rc.resize_draw_contents);
	} else if (!strcasecmp(nodename, "cornerRange.resize")) {
		rc.resize_corner_range = atoi(content);
	} else if (!strcasecmp(nodename, "minimumArea.resize")) {
		rc.resize_minimum_area = MAX(0, atoi(content));
	} else if (!strcasecmp(nodename, "mouseEmulation.tablet")) {
		set_bool(content, &rc.tablet.force_mouse_emulation);
	} else if (!strcasecmp(nodename, "mapToOutput.tablet")) {
		xstrdup_replace(rc.tablet.output_name, content);
	} else if (!strcasecmp(nodename, "rotate.tablet")) {
		rc.tablet.rotation = tablet_parse_rotation(atoi(content));
	} else if (!strcasecmp(nodename, "left.area.tablet")) {
		rc.tablet.box.x = tablet_get_dbl_if_positive(content, "left");
	} else if (!strcasecmp(nodename, "top.area.tablet")) {
		rc.tablet.box.y = tablet_get_dbl_if_positive(content, "top");
	} else if (!strcasecmp(nodename, "width.area.tablet")) {
		rc.tablet.box.width = tablet_get_dbl_if_positive(content, "width");
	} else if (!strcasecmp(nodename, "height.area.tablet")) {
		rc.tablet.box.height = tablet_get_dbl_if_positive(content, "height");
	} else if (!strcasecmp(nodename, "button.map.tablet")) {
		button_map_from = tablet_button_from_str(content);
	} else if (!strcasecmp(nodename, "to.map.tablet")) {
		if (button_map_from != UINT32_MAX) {
			uint32_t button_map_to = mousebind_button_from_str(content, NULL);
			if (button_map_to != UINT32_MAX) {
				tablet_button_mapping_add(button_map_from, button_map_to);
			}
		} else {
			wlr_log(WLR_ERROR, "Missing 'button' argument for tablet button mapping");
		}
	} else if (!strcasecmp(nodename, "motion.tabletTool")) {
		rc.tablet_tool.motion = tablet_parse_motion(content);
	} else if (!strcasecmp(nodename, "relativeMotionSensitivity.tabletTool")) {
		rc.tablet_tool.relative_motion_sensitivity =
			tablet_get_dbl_if_positive(content, "relativeMotionSensitivity");
	} else if (!strcasecmp(nodename, "ignoreButtonReleasePeriod.menu")) {
		rc.menu_ignore_button_release_period = atoi(content);
	} else if (!strcasecmp(nodename, "showIcons.menu")) {
		set_bool(content, &rc.menu_show_icons);
	} else if (!strcasecmp(nodename, "width.magnifier")) {
		rc.mag_width = atoi(content);
	} else if (!strcasecmp(nodename, "height.magnifier")) {
		rc.mag_height = atoi(content);
	} else if (!strcasecmp(nodename, "initScale.magnifier")) {
		set_float(content, &rc.mag_scale);
	} else if (!strcasecmp(nodename, "increment.magnifier")) {
		set_float(content, &rc.mag_increment);
	} else if (!strcasecmp(nodename, "useFilter.magnifier")) {
		set_bool(content, &rc.mag_filter);
	}
}

static void
process_node(xmlNode *node, struct parser_state *state)
{
	char *content;
	static char buffer[256];
	char *name;

	content = (char *)node->content;
	if (xmlIsBlankNode(node)) {
		return;
	}
	name = nodename(node, buffer, sizeof(buffer));
	entry(node, name, content, state);
}

static void xml_tree_walk(xmlNode *node, struct parser_state *state);

static void
traverse(xmlNode *n, struct parser_state *state)
{
	xmlAttr *attr;

	process_node(n, state);
	for (attr = n->properties; attr; attr = attr->next) {
		xml_tree_walk(attr->children, state);
	}
	xml_tree_walk(n->children, state);
}

static void
xml_tree_walk(xmlNode *node, struct parser_state *state)
{
	for (xmlNode *n = node; n && n->name; n = n->next) {
		if (!strcasecmp((char *)n->name, "comment")) {
			continue;
		}
		if (!strcasecmp((char *)n->name, "margin")) {
			state->in_usable_area_override = true;
			traverse(n, state);
			state->in_usable_area_override = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "keybind")) {
			state->in_keybind = true;
			traverse(n, state);
			state->in_keybind = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "mousebind")) {
			state->in_mousebind = true;
			traverse(n, state);
			state->in_mousebind = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "touch")) {
			state->in_touch = true;
			traverse(n, state);
			state->in_touch = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "device")) {
			state->in_libinput_category = true;
			traverse(n, state);
			state->in_libinput_category = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "regions")) {
			state->in_regions = true;
			traverse(n, state);
			state->in_regions = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "fields")) {
			state->in_window_switcher_field = true;
			traverse(n, state);
			state->in_window_switcher_field = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "windowRules")) {
			state->in_window_rules = true;
			traverse(n, state);
			state->in_window_rules = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "query")) {
			state->in_action_query = true;
			traverse(n, state);
			state->in_action_query = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "then")) {
			state->in_action_then_branch = true;
			traverse(n, state);
			state->in_action_then_branch = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "else")) {
			state->in_action_else_branch = true;
			traverse(n, state);
			state->in_action_else_branch = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "none")) {
			state->in_action_none_branch = true;
			traverse(n, state);
			state->in_action_none_branch = false;
			continue;
		}
		traverse(n, state);
	}
}

/* Exposed in header file to allow unit tests to parse buffers */
void
rcxml_parse_xml(struct buf *b)
{
	int options = 0;
	xmlDoc *d = xmlReadMemory(b->data, b->len, NULL, NULL, options);
	if (!d) {
		wlr_log(WLR_ERROR, "error parsing config file");
		return;
	}
	struct parser_state init_state = {0};
	xml_tree_walk(xmlDocGetRootElement(d), &init_state);
	xmlFreeDoc(d);
	xmlCleanupParser();
}

static void
init_font_defaults(struct font *font)
{
	font->size = 10;
	font->slant = FONT_SLANT_NORMAL;
	font->weight = FONT_WEIGHT_NORMAL;
}

static void
rcxml_init(void)
{
	static bool has_run;

	if (!has_run) {
		wl_list_init(&rc.title_buttons_left);
		wl_list_init(&rc.title_buttons_right);
		wl_list_init(&rc.usable_area_overrides);
		wl_list_init(&rc.keybinds);
		wl_list_init(&rc.mousebinds);
		wl_list_init(&rc.libinput_categories);
		wl_list_init(&rc.workspace_config.workspaces);
		wl_list_init(&rc.regions);
		wl_list_init(&rc.window_switcher.fields);
		wl_list_init(&rc.window_rules);
		wl_list_init(&rc.touch_configs);
	}
	has_run = true;

	rc.placement_policy = LAB_PLACE_CASCADE;
	rc.placement_cascade_offset_x = 0;
	rc.placement_cascade_offset_y = 0;

	rc.xdg_shell_server_side_deco = true;
	rc.show_title = true;
	rc.title_layout_loaded = false;
	rc.ssd_keep_border = true;
	rc.corner_radius = 8;
	rc.shadows_enabled = false;

	rc.gap = 0;
	rc.adaptive_sync = LAB_ADAPTIVE_SYNC_DISABLED;
	rc.allow_tearing = false;
	rc.auto_enable_outputs = true;
	rc.reuse_output_mode = false;

#if LAB_WLR_VERSION_OLDER_THAN(0, 18, 2)
	/*
	 * For wlroots < 0.18.2, keep xwayland alive by default to work around
	 * a fatal crash when the X server is terminated during drag-and-drop.
	 */
	rc.xwayland_persistence = true;
#else
	rc.xwayland_persistence = false;
#endif

	init_font_defaults(&rc.font_activewindow);
	init_font_defaults(&rc.font_inactivewindow);
	init_font_defaults(&rc.font_menuheader);
	init_font_defaults(&rc.font_menuitem);
	init_font_defaults(&rc.font_osd);

	rc.focus_follow_mouse = false;
	rc.focus_follow_mouse_requires_movement = true;
	rc.raise_on_focus = false;

	rc.doubleclick_time = 500;

	rc.tablet.force_mouse_emulation = false;
	rc.tablet.output_name = NULL;
	rc.tablet.rotation = 0;
	rc.tablet.box = (struct wlr_fbox){0};
	tablet_load_default_button_mappings();
	rc.tablet_tool.motion = LAB_TABLET_MOTION_ABSOLUTE;
	rc.tablet_tool.relative_motion_sensitivity = 1.0;

	rc.repeat_rate = 25;
	rc.repeat_delay = 600;
	rc.kb_numlock_enable = LAB_STATE_UNSPECIFIED;
	rc.kb_layout_per_window = false;
	rc.screen_edge_strength = 20;
	rc.window_edge_strength = 20;
	rc.unsnap_threshold = 20;
	rc.unmaximize_threshold = 150;

	rc.snap_edge_range = 1;
	rc.snap_overlay_enabled = true;
	rc.snap_overlay_delay_inner = 500;
	rc.snap_overlay_delay_outer = 500;
	rc.snap_top_maximize = true;
	rc.snap_tiling_events_mode = LAB_TILING_EVENTS_ALWAYS;

	rc.window_switcher.show = true;
	rc.window_switcher.preview = true;
	rc.window_switcher.outlines = true;
	rc.window_switcher.criteria = LAB_VIEW_CRITERIA_CURRENT_WORKSPACE
		| LAB_VIEW_CRITERIA_ROOT_TOPLEVEL
		| LAB_VIEW_CRITERIA_NO_SKIP_WINDOW_SWITCHER;

	rc.resize_indicator = LAB_RESIZE_INDICATOR_NEVER;
	rc.resize_draw_contents = true;
	rc.resize_corner_range = -1;
	rc.resize_minimum_area = 8;

	rc.workspace_config.popuptime = INT_MIN;
	rc.workspace_config.min_nr_workspaces = 1;

	rc.menu_ignore_button_release_period = 250;
	rc.menu_show_icons = true;

	rc.mag_width = 400;
	rc.mag_height = 400;
	rc.mag_scale = 2.0;
	rc.mag_increment = 0.2;
	rc.mag_filter = true;
}

static void
load_default_key_bindings(void)
{
	struct keybind *k;
	struct action *action;
	for (int i = 0; key_combos[i].binding; i++) {
		struct key_combos *current = &key_combos[i];
		k = keybind_create(current->binding);
		if (!k) {
			continue;
		}

		action = action_create(current->action);
		wl_list_append(&k->actions, &action->link);

		for (size_t j = 0; j < ARRAY_SIZE(current->attributes); j++) {
			if (!current->attributes[j].name
					|| !current->attributes[j].value) {
				break;
			}
			action_arg_from_xml_node(action,
				current->attributes[j].name,
				current->attributes[j].value);
		}
	}
}

static void
load_default_mouse_bindings(void)
{
	uint32_t count = 0;
	struct mousebind *m;
	struct action *action;
	for (int i = 0; mouse_combos[i].context; i++) {
		struct mouse_combos *current = &mouse_combos[i];
		if (i == 0
				|| strcmp(current->context, mouse_combos[i - 1].context)
				|| strcmp(current->button, mouse_combos[i - 1].button)
				|| strcmp(current->event, mouse_combos[i - 1].event)) {
			/* Create new mousebind */
			m = mousebind_create(current->context);
			m->mouse_event = mousebind_event_from_str(current->event);
			if (m->mouse_event == MOUSE_ACTION_SCROLL) {
				m->direction = mousebind_direction_from_str(current->button,
					&m->modifiers);
			} else {
				m->button = mousebind_button_from_str(current->button,
					&m->modifiers);
			}
			count++;
		}

		action = action_create(current->action);
		wl_list_append(&m->actions, &action->link);

		for (size_t j = 0; j < ARRAY_SIZE(current->attributes); j++) {
			if (!current->attributes[j].name
					|| !current->attributes[j].value) {
				break;
			}
			action_arg_from_xml_node(action,
				current->attributes[j].name,
				current->attributes[j].value);
		}
	}
	wlr_log(WLR_DEBUG, "Loaded %u merged mousebinds", count);
}

static void
deduplicate_mouse_bindings(void)
{
	uint32_t replaced = 0;
	uint32_t cleared = 0;
	struct mousebind *current, *tmp, *existing;
	wl_list_for_each_safe(existing, tmp, &rc.mousebinds, link) {
		wl_list_for_each_reverse(current, &rc.mousebinds, link) {
			if (existing == current) {
				break;
			}
			if (mousebind_the_same(existing, current)) {
				wl_list_remove(&existing->link);
				action_list_free(&existing->actions);
				free(existing);
				replaced++;
				break;
			}
		}
	}
	wl_list_for_each_safe(current, tmp, &rc.mousebinds, link) {
		if (wl_list_empty(&current->actions)) {
			wl_list_remove(&current->link);
			free(current);
			cleared++;
		}
	}
	if (replaced) {
		wlr_log(WLR_DEBUG, "Replaced %u mousebinds", replaced);
	}
	if (cleared) {
		wlr_log(WLR_DEBUG, "Cleared %u mousebinds", cleared);
	}
}

static void
deduplicate_key_bindings(void)
{
	uint32_t replaced = 0;
	uint32_t cleared = 0;
	struct keybind *current, *tmp, *existing;
	wl_list_for_each_safe(existing, tmp, &rc.keybinds, link) {
		wl_list_for_each_reverse(current, &rc.keybinds, link) {
			if (existing == current) {
				break;
			}
			if (keybind_the_same(existing, current)) {
				wl_list_remove(&existing->link);
				action_list_free(&existing->actions);
				keybind_destroy(existing);
				replaced++;
				break;
			}
		}
	}
	wl_list_for_each_safe(current, tmp, &rc.keybinds, link) {
		if (wl_list_empty(&current->actions)) {
			wl_list_remove(&current->link);
			keybind_destroy(current);
			cleared++;
		}
	}
	if (replaced) {
		wlr_log(WLR_DEBUG, "Replaced %u keybinds", replaced);
	}
	if (cleared) {
		wlr_log(WLR_DEBUG, "Cleared %u keybinds", cleared);
	}
}

static struct {
	enum window_switcher_field_content content;
	int width;
} fields[] = {
	{ LAB_FIELD_TYPE, 25 },
	{ LAB_FIELD_TRIMMED_IDENTIFIER, 25 },
	{ LAB_FIELD_TITLE, 50 },
	{ LAB_FIELD_NONE, 0 },
};

static void
load_default_window_switcher_fields(void)
{
	struct window_switcher_field *field;

	for (int i = 0; fields[i].content != LAB_FIELD_NONE; i++) {
		field = znew(*field);
		field->content = fields[i].content;
		field->width = fields[i].width;
		wl_list_append(&rc.window_switcher.fields, &field->link);
	}
}

static void
post_processing(void)
{
	if (!wl_list_length(&rc.keybinds)) {
		wlr_log(WLR_INFO, "load default key bindings");
		load_default_key_bindings();
	}

	if (!wl_list_length(&rc.mousebinds)) {
		wlr_log(WLR_INFO, "load default mouse bindings");
		load_default_mouse_bindings();
	}

	if (!rc.fallback_app_icon_name) {
		rc.fallback_app_icon_name = xstrdup("labwc");
	}

	if (!rc.icon_theme_name && rc.theme_name) {
		rc.icon_theme_name = xstrdup(rc.theme_name);
	}

	if (!rc.title_layout_loaded) {
#if HAVE_LIBSFDO
		fill_title_layout("icon:iconify,max,close");
#else
		/*
		 * 'icon' is replaced with 'menu' in fill_title_layout() when
		 * libsfdo is not linked, but we also replace it here not to
		 * show error message with default settings.
		 */
		fill_title_layout("menu:iconify,max,close");
#endif
	}

	/*
	 * Replace all earlier bindings by later ones
	 * and clear the ones with an empty action list.
	 *
	 * This is required so users are able to remove
	 * a default binding by using the "None" action.
	 */
	deduplicate_key_bindings();
	deduplicate_mouse_bindings();

	if (!rc.font_activewindow.name) {
		rc.font_activewindow.name = xstrdup("sans");
	}
	if (!rc.font_inactivewindow.name) {
		rc.font_inactivewindow.name = xstrdup("sans");
	}
	if (!rc.font_menuheader.name) {
		rc.font_menuheader.name = xstrdup("sans");
	}
	if (!rc.font_menuitem.name) {
		rc.font_menuitem.name = xstrdup("sans");
	}
	if (!rc.font_osd.name) {
		rc.font_osd.name = xstrdup("sans");
	}
	if (!libinput_category_get_default()) {
		/* So we set default values of <tap> and <scrollFactor> */
		struct libinput_category *l = libinput_category_create();
		/* Prevents unused variable warning when compiled without asserts */
		(void)l;
		assert(l && libinput_category_get_default() == l);
	}
	if (mouse_scroll_factor >= 0) {
		wlr_log(WLR_ERROR, "<mouse><scrollFactor> is deprecated"
				" and overwrites <libinput><scrollFactor>."
				" Use only <libinput><scrollFactor>.");
		struct libinput_category *l;
		wl_list_for_each(l, &rc.libinput_categories, link) {
			l->scroll_factor = mouse_scroll_factor;
		}
	}

	int nr_workspaces = wl_list_length(&rc.workspace_config.workspaces);
	if (nr_workspaces < rc.workspace_config.min_nr_workspaces) {
		if (!rc.workspace_config.prefix) {
			rc.workspace_config.prefix = xstrdup(_("Workspace"));
		}
		struct workspace *workspace;
		for (int i = nr_workspaces; i < rc.workspace_config.min_nr_workspaces; i++) {
			workspace = znew(*workspace);
			workspace->name = strdup_printf("%s %d",
				rc.workspace_config.prefix, i + 1);
			wl_list_append(&rc.workspace_config.workspaces, &workspace->link);
		}
	}
	if (rc.workspace_config.popuptime == INT_MIN) {
		rc.workspace_config.popuptime = 1000;
	}
	if (!wl_list_length(&rc.window_switcher.fields)) {
		wlr_log(WLR_INFO, "load default window switcher fields");
		load_default_window_switcher_fields();
	}

	if (rc.mag_scale <= 0.0) {
		rc.mag_scale = 1.0;
	}
}

static void
rule_destroy(struct window_rule *rule)
{
	wl_list_remove(&rule->link);
	zfree(rule->identifier);
	zfree(rule->title);
	zfree(rule->sandbox_engine);
	zfree(rule->sandbox_app_id);
	action_list_free(&rule->actions);
	zfree(rule);
}

static void
validate_actions(void)
{
	struct action *action, *action_tmp;

	struct keybind *keybind;
	wl_list_for_each(keybind, &rc.keybinds, link) {
		wl_list_for_each_safe(action, action_tmp, &keybind->actions, link) {
			if (!action_is_valid(action)) {
				wl_list_remove(&action->link);
				action_free(action);
				wlr_log(WLR_ERROR, "Removed invalid keybind action");
			}
		}
	}

	struct mousebind *mousebind;
	wl_list_for_each(mousebind, &rc.mousebinds, link) {
		wl_list_for_each_safe(action, action_tmp, &mousebind->actions, link) {
			if (!action_is_valid(action)) {
				wl_list_remove(&action->link);
				action_free(action);
				wlr_log(WLR_ERROR, "Removed invalid mousebind action");
			}
		}
	}

	struct window_rule *rule;
	wl_list_for_each(rule, &rc.window_rules, link) {
		wl_list_for_each_safe(action, action_tmp, &rule->actions, link) {
			if (!action_is_valid(action)) {
				wl_list_remove(&action->link);
				action_free(action);
				wlr_log(WLR_ERROR, "Removed invalid window rule action");
			}
		}
	}
}

static void
validate(void)
{
	/* Regions */
	struct region *region, *region_tmp;
	wl_list_for_each_safe(region, region_tmp, &rc.regions, link) {
		struct wlr_box box = region->percentage;
		bool invalid = !region->name
			|| box.x < 0 || box.x > 100
			|| box.y < 0 || box.y > 100
			|| box.width <= 0 || box.width > 100
			|| box.height <= 0 || box.height > 100;
		if (invalid) {
			wlr_log(WLR_ERROR,
				"Removing invalid region '%s': %d%% x %d%% @ %d%%,%d%%",
				region->name, box.width, box.height, box.x, box.y);
			wl_list_remove(&region->link);
			zfree(region->name);
			free(region);
		}
	}

	/* Window-rule criteria */
	struct window_rule *rule, *rule_tmp;
	wl_list_for_each_safe(rule, rule_tmp, &rc.window_rules, link) {
		if (!rule->identifier && !rule->title && rule->window_type < 0
				&& !rule->sandbox_engine && !rule->sandbox_app_id) {
			wlr_log(WLR_ERROR, "Deleting rule %p as it has no criteria", rule);
			rule_destroy(rule);
		}
	}

	validate_actions();

	/* OSD fields */
	struct window_switcher_field *field, *field_tmp;
	wl_list_for_each_safe(field, field_tmp, &rc.window_switcher.fields, link) {
		if (!osd_field_validate(field)) {
			wlr_log(WLR_ERROR, "Deleting invalid window switcher field %p", field);
			wl_list_remove(&field->link);
			osd_field_free(field);
		}
	}
}

void
rcxml_read(const char *filename)
{
	rcxml_init();

	struct wl_list paths;

	if (filename) {
		/* Honour command line argument -c <filename> */
		wl_list_init(&paths);
		struct path *path = znew(*path);
		path->string = xstrdup(filename);
		wl_list_append(&paths, &path->link);
	} else {
		paths_config_create(&paths, "rc.xml");
	}

	/* Reading file into buffer before parsing - better for unit tests */
	bool should_merge_config = rc.merge_config;
	struct wl_list *(*iter)(struct wl_list *list);
	iter = should_merge_config ? paths_get_prev : paths_get_next;

	/*
	 * This is the equivalent of a wl_list_for_each() which optionally
	 * iterates in reverse depending on 'should_merge_config'
	 *
	 * If not merging, we iterate forwards and break after the first
	 * iteration.
	 *
	 * If merging, we iterate backwards (least important XDG Base Dir first)
	 * and keep going.
	 */
	for (struct wl_list *elm = iter(&paths); elm != &paths; elm = iter(elm)) {
		struct path *path = wl_container_of(elm, path, link);
		FILE *stream = fopen(path->string, "r");
		if (!stream) {
			continue;
		}

		wlr_log(WLR_INFO, "read config file %s", path->string);

		struct buf b = BUF_INIT;
		char *line = NULL;
		size_t len = 0;
		while (getline(&line, &len, stream) != -1) {
			char *p = strrchr(line, '\n');
			if (p) {
				*p = '\0';
			}
			buf_add(&b, line);
		}
		zfree(line);
		fclose(stream);
		rcxml_parse_xml(&b);
		buf_reset(&b);
		if (!should_merge_config) {
			break;
		}
	};
	paths_destroy(&paths);
	post_processing();
	validate();
}

void
rcxml_finish(void)
{
	zfree(rc.font_activewindow.name);
	zfree(rc.font_inactivewindow.name);
	zfree(rc.font_menuheader.name);
	zfree(rc.font_menuitem.name);
	zfree(rc.font_osd.name);
	zfree(rc.theme_name);
	zfree(rc.icon_theme_name);
	zfree(rc.fallback_app_icon_name);
	zfree(rc.workspace_config.prefix);
	zfree(rc.tablet.output_name);

	struct title_button *p, *p_tmp;
	wl_list_for_each_safe(p, p_tmp, &rc.title_buttons_left, link) {
		wl_list_remove(&p->link);
		zfree(p);
	}
	wl_list_for_each_safe(p, p_tmp, &rc.title_buttons_right, link) {
		wl_list_remove(&p->link);
		zfree(p);
	}

	struct usable_area_override *area, *area_tmp;
	wl_list_for_each_safe(area, area_tmp, &rc.usable_area_overrides, link) {
		wl_list_remove(&area->link);
		zfree(area->output);
		zfree(area);
	}

	struct keybind *k, *k_tmp;
	wl_list_for_each_safe(k, k_tmp, &rc.keybinds, link) {
		wl_list_remove(&k->link);
		action_list_free(&k->actions);
		keybind_destroy(k);
	}

	struct mousebind *m, *m_tmp;
	wl_list_for_each_safe(m, m_tmp, &rc.mousebinds, link) {
		wl_list_remove(&m->link);
		action_list_free(&m->actions);
		zfree(m);
	}

	struct touch_config_entry *touch_config, *touch_config_tmp;
	wl_list_for_each_safe(touch_config, touch_config_tmp, &rc.touch_configs, link) {
		wl_list_remove(&touch_config->link);
		zfree(touch_config->device_name);
		zfree(touch_config->output_name);
		zfree(touch_config);
	}

	struct libinput_category *l, *l_tmp;
	wl_list_for_each_safe(l, l_tmp, &rc.libinput_categories, link) {
		wl_list_remove(&l->link);
		zfree(l->name);
		zfree(l);
	}

	struct workspace *w, *w_tmp;
	wl_list_for_each_safe(w, w_tmp, &rc.workspace_config.workspaces, link) {
		wl_list_remove(&w->link);
		zfree(w->name);
		zfree(w);
	}

	regions_destroy(NULL, &rc.regions);

	struct window_switcher_field *field, *field_tmp;
	wl_list_for_each_safe(field, field_tmp, &rc.window_switcher.fields, link) {
		wl_list_remove(&field->link);
		osd_field_free(field);
	}

	struct window_rule *rule, *rule_tmp;
	wl_list_for_each_safe(rule, rule_tmp, &rc.window_rules, link) {
		rule_destroy(rule);
	}

	/* Reset state vars for starting fresh when Reload is triggered */
	mouse_scroll_factor = -1;
}
