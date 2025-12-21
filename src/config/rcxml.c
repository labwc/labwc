// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "config/rcxml.h"
#include <assert.h>
#include <glib.h>
#include <libxml/parser.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include "action.h"
#include "common/buf.h"
#include "common/dir.h"
#include "common/list.h"
#include "common/macros.h"
#include "common/mem.h"
#include "common/nodename.h"
#include "common/parse-bool.h"
#include "common/parse-double.h"
#include "common/string-helpers.h"
#include "common/xml.h"
#include "config/default-bindings.h"
#include "config/keybind.h"
#include "config/libinput.h"
#include "config/mousebind.h"
#include "config/tablet.h"
#include "config/tablet-tool.h"
#include "config/touch.h"
#include "cycle.h"
#include "labwc.h"
#include "regions.h"
#include "ssd.h"
#include "translate.h"
#include "view.h"
#include "window-rules.h"
#include "workspaces.h"

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

static enum lab_window_type
parse_window_type(const char *type)
{
	if (!type) {
		return LAB_WINDOW_TYPE_INVALID;
	}
	if (!strcasecmp(type, "desktop")) {
		return LAB_WINDOW_TYPE_DESKTOP;
	} else if (!strcasecmp(type, "dock")) {
		return LAB_WINDOW_TYPE_DOCK;
	} else if (!strcasecmp(type, "toolbar")) {
		return LAB_WINDOW_TYPE_TOOLBAR;
	} else if (!strcasecmp(type, "menu")) {
		return LAB_WINDOW_TYPE_MENU;
	} else if (!strcasecmp(type, "utility")) {
		return LAB_WINDOW_TYPE_UTILITY;
	} else if (!strcasecmp(type, "splash")) {
		return LAB_WINDOW_TYPE_SPLASH;
	} else if (!strcasecmp(type, "dialog")) {
		return LAB_WINDOW_TYPE_DIALOG;
	} else if (!strcasecmp(type, "dropdown_menu")) {
		return LAB_WINDOW_TYPE_DROPDOWN_MENU;
	} else if (!strcasecmp(type, "popup_menu")) {
		return LAB_WINDOW_TYPE_POPUP_MENU;
	} else if (!strcasecmp(type, "tooltip")) {
		return LAB_WINDOW_TYPE_TOOLTIP;
	} else if (!strcasecmp(type, "notification")) {
		return LAB_WINDOW_TYPE_NOTIFICATION;
	} else if (!strcasecmp(type, "combo")) {
		return LAB_WINDOW_TYPE_COMBO;
	} else if (!strcasecmp(type, "dnd")) {
		return LAB_WINDOW_TYPE_DND;
	} else if (!strcasecmp(type, "normal")) {
		return LAB_WINDOW_TYPE_NORMAL;
	} else {
		return LAB_WINDOW_TYPE_INVALID;
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
fill_section(const char *content, enum lab_node_type *buttons, int *count,
		uint32_t *found_buttons /* bitmask */)
{
	gchar **identifiers = g_strsplit(content, ",", -1);
	for (size_t i = 0; identifiers[i]; ++i) {
		char *identifier = identifiers[i];
		if (string_null_or_empty(identifier)) {
			continue;
		}
		enum lab_node_type type = LAB_NODE_NONE;
		if (!strcmp(identifier, "icon")) {
#if HAVE_LIBSFDO
			type = LAB_NODE_BUTTON_WINDOW_ICON;
#else
			wlr_log(WLR_ERROR, "libsfdo is not linked. "
				"Replacing 'icon' in titlebar layout with 'menu'.");
			type = LAB_NODE_BUTTON_WINDOW_MENU;
#endif
		} else if (!strcmp(identifier, "menu")) {
			type = LAB_NODE_BUTTON_WINDOW_MENU;
		} else if (!strcmp(identifier, "iconify")) {
			type = LAB_NODE_BUTTON_ICONIFY;
		} else if (!strcmp(identifier, "max")) {
			type = LAB_NODE_BUTTON_MAXIMIZE;
		} else if (!strcmp(identifier, "close")) {
			type = LAB_NODE_BUTTON_CLOSE;
		} else if (!strcmp(identifier, "shade")) {
			type = LAB_NODE_BUTTON_SHADE;
		} else if (!strcmp(identifier, "desk")) {
			type = LAB_NODE_BUTTON_OMNIPRESENT;
		} else {
			wlr_log(WLR_ERROR, "invalid titleLayout identifier '%s'",
				identifier);
			continue;
		}

		assert(type != LAB_NODE_NONE);

		/* We no longer need this check, but let's keep it just in case */
		if (*found_buttons & (1 << type)) {
			wlr_log(WLR_ERROR, "ignoring duplicated button type '%s'",
				identifier);
			continue;
		}

		*found_buttons |= (1 << type);

		assert(*count < TITLE_BUTTONS_MAX);
		buttons[(*count)++] = type;
	}
	g_strfreev(identifiers);
}

static void
clear_title_layout(void)
{
	rc.nr_title_buttons_left = 0;
	rc.nr_title_buttons_right = 0;
	rc.title_layout_loaded = false;
}

static void
fill_title_layout(const char *content)
{
	clear_title_layout();

	gchar **parts = g_strsplit(content, ":", -1);

	if (g_strv_length(parts) != 2) {
		wlr_log(WLR_ERROR, "<titlebar><layout> must contain one colon");
		goto err;
	}

	uint32_t found_buttons = 0;
	fill_section(parts[0], rc.title_buttons_left,
		&rc.nr_title_buttons_left, &found_buttons);
	fill_section(parts[1], rc.title_buttons_right,
		&rc.nr_title_buttons_right, &found_buttons);

	rc.title_layout_loaded = true;
err:
	g_strfreev(parts);
}

static void
fill_usable_area_override(xmlNode *node)
{
	struct usable_area_override *usable_area_override =
		znew(*usable_area_override);
	wl_list_append(&rc.usable_area_overrides, &usable_area_override->link);

	xmlNode *child;
	char *key, *content;
	LAB_XML_FOR_EACH(node, child, key, content) {
		if (!strcmp(key, "output")) {
			xstrdup_replace(usable_area_override->output, content);
		} else if (!strcmp(key, "left")) {
			usable_area_override->margin.left = atoi(content);
		} else if (!strcmp(key, "right")) {
			usable_area_override->margin.right = atoi(content);
		} else if (!strcmp(key, "top")) {
			usable_area_override->margin.top = atoi(content);
		} else if (!strcmp(key, "bottom")) {
			usable_area_override->margin.bottom = atoi(content);
		} else {
			wlr_log(WLR_ERROR, "Unexpected data usable-area-override "
				"parser: %s=\"%s\"", key, content);
		}
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
fill_window_rule(xmlNode *node)
{
	struct window_rule *window_rule = znew(*window_rule);
	window_rule->window_type = LAB_WINDOW_TYPE_INVALID;
	wl_list_append(&rc.window_rules, &window_rule->link);
	wl_list_init(&window_rule->actions);

	xmlNode *child;
	char *key, *content;
	LAB_XML_FOR_EACH(node, child, key, content) {
		/* Criteria */
		if (!strcmp(key, "identifier")) {
			xstrdup_replace(window_rule->identifier, content);
		} else if (!strcmp(key, "title")) {
			xstrdup_replace(window_rule->title, content);
		} else if (!strcmp(key, "type")) {
			window_rule->window_type = parse_window_type(content);
		} else if (!strcasecmp(key, "matchOnce")) {
			set_bool(content, &window_rule->match_once);
		} else if (!strcasecmp(key, "sandboxEngine")) {
			xstrdup_replace(window_rule->sandbox_engine, content);
		} else if (!strcasecmp(key, "sandboxAppId")) {
			xstrdup_replace(window_rule->sandbox_app_id, content);

		/* Event */
		} else if (!strcmp(key, "event")) {
			/*
			 * This is just in readiness for adding any other types of
			 * events in the future. We default to onFirstMap anyway.
			 */
			if (!strcasecmp(content, "onFirstMap")) {
				window_rule->event = LAB_WINDOW_RULE_EVENT_ON_FIRST_MAP;
			}

		/* Properties */
		} else if (!strcasecmp(key, "serverDecoration")) {
			set_property(content, &window_rule->server_decoration);
		} else if (!strcasecmp(key, "iconPriority")) {
			if (!strcasecmp(content, "client")) {
				window_rule->icon_prefer_client = LAB_PROP_TRUE;
			} else if (!strcasecmp(content, "server")) {
				window_rule->icon_prefer_client = LAB_PROP_FALSE;
			} else {
				wlr_log(WLR_ERROR,
					"Invalid value for window rule property 'iconPriority'");
			}
		} else if (!strcasecmp(key, "skipTaskbar")) {
			set_property(content, &window_rule->skip_taskbar);
		} else if (!strcasecmp(key, "skipWindowSwitcher")) {
			set_property(content, &window_rule->skip_window_switcher);
		} else if (!strcasecmp(key, "ignoreFocusRequest")) {
			set_property(content, &window_rule->ignore_focus_request);
		} else if (!strcasecmp(key, "ignoreConfigureRequest")) {
			set_property(content, &window_rule->ignore_configure_request);
		} else if (!strcasecmp(key, "fixedPosition")) {
			set_property(content, &window_rule->fixed_position);
		}
	}

	append_parsed_actions(node, &window_rule->actions);
}

static void
fill_window_rules(xmlNode *node)
{
	/* TODO: make sure <windowRules> is empty here */

	xmlNode *child;
	char *key, *content;
	LAB_XML_FOR_EACH(node, child, key, content) {
		if (!strcasecmp(key, "windowRule")) {
			fill_window_rule(child);
		}
	}
}

static void
clear_window_switcher_fields(void)
{
	struct cycle_osd_field *field, *field_tmp;
	wl_list_for_each_safe(field, field_tmp, &rc.window_switcher.fields, link) {
		wl_list_remove(&field->link);
		cycle_osd_field_free(field);
	}
}

static void
fill_window_switcher_field(xmlNode *node)
{
	struct cycle_osd_field *field = znew(*field);
	wl_list_append(&rc.window_switcher.fields, &field->link);

	xmlNode *child;
	char *key, *content;
	LAB_XML_FOR_EACH(node, child, key, content) {
		cycle_osd_field_arg_from_xml_node(field, key, content);
	}
}

static void
fill_window_switcher_fields(xmlNode *node)
{
	clear_window_switcher_fields();

	xmlNode *child;
	char *key, *content;
	LAB_XML_FOR_EACH(node, child, key, content) {
		if (!strcasecmp(key, "field")) {
			fill_window_switcher_field(child);
		}
	}
}

static void
fill_region(xmlNode *node)
{
	struct region *region = znew(*region);
	wl_list_append(&rc.regions, &region->link);

	xmlNode *child;
	char *key, *content;
	LAB_XML_FOR_EACH(node, child, key, content) {
		if (!strcasecmp(key, "name")) {
			xstrdup_replace(region->name, content);
		} else if (strstr("xywidtheight", key)
				&& !strchr(content, '%')) {
			wlr_log(WLR_ERROR, "Removing invalid region "
				"'%s': %s='%s' misses a trailing %%",
				region->name, key, content);
			wl_list_remove(&region->link);
			zfree(region->name);
			zfree(region);
			return;
		} else if (!strcmp(key, "x")) {
			region->percentage.x = atoi(content);
		} else if (!strcmp(key, "y")) {
			region->percentage.y = atoi(content);
		} else if (!strcmp(key, "width")) {
			region->percentage.width = atoi(content);
		} else if (!strcmp(key, "height")) {
			region->percentage.height = atoi(content);
		} else {
			wlr_log(WLR_ERROR, "Unexpected data in region "
				"parser: %s=\"%s\"", key, content);
		}
	}
}

static void
fill_regions(xmlNode *node)
{
	/* TODO: make sure <regions> is empty here */

	xmlNode *child;
	char *key, *content;
	LAB_XML_FOR_EACH(node, child, key, content) {
		if (!strcasecmp(key, "region")) {
			fill_region(child);
		}
	}
}

static void
fill_action_query(struct action *action, xmlNode *node, struct view_query *query)
{
	xmlNode *child;
	char *key, *content;
	LAB_XML_FOR_EACH(node, child, key, content) {
		if (!strcasecmp(key, "identifier")) {
			xstrdup_replace(query->identifier, content);
		} else if (!strcasecmp(key, "title")) {
			xstrdup_replace(query->title, content);
		} else if (!strcmp(key, "type")) {
			query->window_type = parse_window_type(content);
		} else if (!strcasecmp(key, "sandboxEngine")) {
			xstrdup_replace(query->sandbox_engine, content);
		} else if (!strcasecmp(key, "sandboxAppId")) {
			xstrdup_replace(query->sandbox_app_id, content);
		} else if (!strcasecmp(key, "shaded")) {
			query->shaded = parse_tristate(content);
		} else if (!strcasecmp(key, "maximized")) {
			query->maximized = view_axis_parse(content);
		} else if (!strcasecmp(key, "iconified")) {
			query->iconified = parse_tristate(content);
		} else if (!strcasecmp(key, "focused")) {
			query->focused = parse_tristate(content);
		} else if (!strcasecmp(key, "omnipresent")) {
			query->omnipresent = parse_tristate(content);
		} else if (!strcasecmp(key, "tiled")) {
			query->tiled = lab_edge_parse(content,
				/*tiled*/ true, /*any*/ true);
		} else if (!strcasecmp(key, "tiled_region")) {
			xstrdup_replace(query->tiled_region, content);
		} else if (!strcasecmp(key, "desktop")) {
			xstrdup_replace(query->desktop, content);
		} else if (!strcasecmp(key, "decoration")) {
			query->decoration = ssd_mode_parse(content);
		} else if (!strcasecmp(key, "monitor")) {
			xstrdup_replace(query->monitor, content);
		}
	}
}

static void
parse_action_args(xmlNode *node, struct action *action)
{
	xmlNode *child;
	char *key, *content;
	LAB_XML_FOR_EACH(node, child, key, content) {
		if (!strcasecmp(key, "query")) {
			struct wl_list *querylist =
				action_get_querylist(action, "query");
			if (!querylist) {
				action_arg_add_querylist(action, "query");
				querylist = action_get_querylist(action, "query");
			}
			struct view_query *query = view_query_create();
			fill_action_query(action, child, query);
			wl_list_append(querylist, &query->link);
		} else if (!strcasecmp(key, "then")) {
			struct wl_list *actions =
				action_get_actionlist(action, "then");
			if (!actions) {
				action_arg_add_actionlist(action, "then");
				actions = action_get_actionlist(action, "then");
			}
			append_parsed_actions(child, actions);
		} else if (!strcasecmp(key, "else")) {
			struct wl_list *actions =
				action_get_actionlist(action, "else");
			if (!actions) {
				action_arg_add_actionlist(action, "else");
				actions = action_get_actionlist(action, "else");
			}
			append_parsed_actions(child, actions);
		} else if (!strcasecmp(key, "none")) {
			struct wl_list *actions =
				action_get_actionlist(action, "none");
			if (!actions) {
				action_arg_add_actionlist(action, "none");
				actions = action_get_actionlist(action, "none");
			}
			append_parsed_actions(child, actions);
		} else if (!strcasecmp(key, "name")) {
			/* Ignore <action name=""> */
		} else if (lab_xml_node_is_leaf(child)) {
			/* Handle normal action args */
			char buffer[256];
			char *node_name = nodename(child, buffer, sizeof(buffer));
			action_arg_from_xml_node(action, node_name, content);
		} else {
			/* Handle nested args like <position><x> in ShowMenu */
			parse_action_args(child, action);
		}
	}
}

static struct action *
parse_action(xmlNode *node)
{
	char name[256];
	struct action *action = NULL;

	if (lab_xml_get_string(node, "name", name, sizeof(name))) {
		action = action_create(name);
	}
	if (!action) {
		return NULL;
	}

	parse_action_args(node, action);
	return action;
}

void
append_parsed_actions(xmlNode *node, struct wl_list *list)
{
	xmlNode *child;
	char *key, *content;
	LAB_XML_FOR_EACH(node, child, key, content) {
		if (strcasecmp(key, "action")) {
			continue;
		}
		if (lab_xml_node_is_leaf(child)) {
			/*
			 * A mousebind contains two types of "action" nodes:
			 *   <mousebind button="Left" action="Click">
			 *     <action name="Close" />
			 *   </mousebind>
			 * The first node (action="Click") is skipped.
			 */
			continue;
		}
		struct action *action = parse_action(child);
		if (action) {
			wl_list_append(list, &action->link);
		}
	}
}

static void
fill_keybind(xmlNode *node)
{
	struct keybind *keybind = NULL;
	char keyname[256];

	if (lab_xml_get_string(node, "key", keyname, sizeof(keyname))) {
		keybind = keybind_create(keyname);
		if (!keybind) {
			wlr_log(WLR_ERROR, "Invalid keybind: %s", keyname);
		}
	}
	if (!keybind) {
		return;
	}

	lab_xml_get_bool(node, "onRelease", &keybind->on_release);
	lab_xml_get_bool(node, "layoutDependent", &keybind->use_syms_only);
	lab_xml_get_bool(node, "allowWhenLocked", &keybind->allow_when_locked);

	append_parsed_actions(node, &keybind->actions);
}

static void
fill_mousebind(xmlNode *node, const char *context)
{
	/*
	 * Example of what we are parsing:
	 * <mousebind button="Left" action="DoubleClick">
	 *   <action name="Focus"/>
	 *   <action name="Raise"/>
	 *   <action name="ToggleMaximize"/>
	 * </mousebind>
	 */

	wlr_log(WLR_INFO, "create mousebind for %s", context);
	struct mousebind *mousebind = mousebind_create(context);

	char buf[256];
	if (lab_xml_get_string(node, "button", buf, sizeof(buf))) {
		mousebind->button = mousebind_button_from_str(
			buf, &mousebind->modifiers);
	}
	if (lab_xml_get_string(node, "direction", buf, sizeof(buf))) {
		mousebind->direction = mousebind_direction_from_str(
			buf, &mousebind->modifiers);
	}
	if (lab_xml_get_string(node, "action", buf, sizeof(buf))) {
		/* <mousebind button="" action="EVENT"> */
		mousebind->mouse_event = mousebind_event_from_str(buf);
	}

	append_parsed_actions(node, &mousebind->actions);
}

static void
fill_mouse_context(xmlNode *node)
{
	char context_name[256];
	if (!lab_xml_get_string(node, "name", context_name, sizeof(context_name))) {
		return;
	}

	xmlNode *child;
	char *key, *content;
	LAB_XML_FOR_EACH(node, child, key, content) {
		if (!strcasecmp(key, "mousebind")) {
			fill_mousebind(child, context_name);
		}
	}
}

static void
fill_touch(xmlNode *node)
{
	struct touch_config_entry *touch_config = znew(*touch_config);
	wl_list_append(&rc.touch_configs, &touch_config->link);

	xmlNode *child;
	char *key, *content;
	LAB_XML_FOR_EACH(node, child, key, content) {
		if (!strcasecmp(key, "deviceName")) {
			xstrdup_replace(touch_config->device_name, content);
		} else if (!strcasecmp(key, "mapToOutput")) {
			xstrdup_replace(touch_config->output_name, content);
		} else if (!strcasecmp(key, "mouseEmulation")) {
			set_bool(content, &touch_config->force_mouse_emulation);
		} else {
			wlr_log(WLR_ERROR, "Unexpected data in touch parser: %s=\"%s\"",
				key, content);
		}
	}
}

static void
fill_tablet_button_map(xmlNode *node)
{
	uint32_t map_from;
	uint32_t map_to;
	char buf[256];

	if (lab_xml_get_string(node, "button", buf, sizeof(buf))) {
		map_from = tablet_button_from_str(buf);
	} else {
		wlr_log(WLR_ERROR, "Invalid 'button' argument for tablet button mapping");
		return;
	}

	if (lab_xml_get_string(node, "to", buf, sizeof(buf))) {
		map_to = mousebind_button_from_str(buf, NULL);
	} else {
		wlr_log(WLR_ERROR, "Invalid 'to' argument for tablet button mapping");
		return;
	}

	tablet_button_mapping_add(map_from, map_to);
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
fill_libinput_category(xmlNode *node)
{
	/*
	 * Create a new profile (libinput-category) on `<libinput><device>`
	 * so that the 'default' profile can be created without even providing a
	 * category="" attribute (same as <device category="default">...)
	 */
	struct libinput_category *category = libinput_category_create();

	xmlNode *child;
	char *key, *content;
	LAB_XML_FOR_EACH(node, child, key, content) {
		if (string_null_or_empty(content)) {
			wlr_log(WLR_ERROR, "Empty string is not allowed for "
				"<libinput><device><%s>. Ignoring.", key);
			continue;
		}
		if (!strcmp(key, "category")) {
			/*
			 * First we try to get a type based on a number of
			 * pre-defined terms, for example: 'default', 'touch',
			 * 'touchpad' and 'non-touch'
			 */
			category->type = get_device_type(content);

			/*
			 * If we couldn't match against any of those terms, we
			 * use the provided value to define the device name
			 * that the settings should be applicable to.
			 */
			if (category->type == LAB_LIBINPUT_DEVICE_NONE) {
				xstrdup_replace(category->name, content);
			}
		} else if (!strcasecmp(key, "naturalScroll")) {
			set_bool_as_int(content, &category->natural_scroll);
		} else if (!strcasecmp(key, "leftHanded")) {
			set_bool_as_int(content, &category->left_handed);
		} else if (!strcasecmp(key, "pointerSpeed")) {
			set_float(content, &category->pointer_speed);
			if (category->pointer_speed < -1) {
				category->pointer_speed = -1;
			} else if (category->pointer_speed > 1) {
				category->pointer_speed = 1;
			}
		} else if (!strcasecmp(key, "tap")) {
			int ret = parse_bool(content, -1);
			if (ret < 0) {
				continue;
			}
			category->tap = ret
				? LIBINPUT_CONFIG_TAP_ENABLED
				: LIBINPUT_CONFIG_TAP_DISABLED;
		} else if (!strcasecmp(key, "tapButtonMap")) {
			if (!strcmp(content, "lrm")) {
				category->tap_button_map =
					LIBINPUT_CONFIG_TAP_MAP_LRM;
			} else if (!strcmp(content, "lmr")) {
				category->tap_button_map =
					LIBINPUT_CONFIG_TAP_MAP_LMR;
			} else {
				wlr_log(WLR_ERROR, "invalid tapButtonMap");
			}
		} else if (!strcasecmp(key, "tapAndDrag")) {
			int ret = parse_bool(content, -1);
			if (ret < 0) {
				continue;
			}
			category->tap_and_drag = ret
				? LIBINPUT_CONFIG_DRAG_ENABLED
				: LIBINPUT_CONFIG_DRAG_DISABLED;
		} else if (!strcasecmp(key, "dragLock")) {
			if (!strcasecmp(content, "timeout")) {
				/* "timeout" enables drag-lock with timeout */
				category->drag_lock = LIBINPUT_CONFIG_DRAG_LOCK_ENABLED;
				continue;
			}
			int ret = parse_bool(content, -1);
			if (ret < 0) {
				continue;
			}
			/* "yes" enables drag-lock, without timeout if libinput >= 1.27 */
			int enabled = LIBINPUT_CONFIG_DRAG_LOCK_ENABLED;
#if HAVE_LIBINPUT_CONFIG_DRAG_LOCK_ENABLED_STICKY
			enabled = LIBINPUT_CONFIG_DRAG_LOCK_ENABLED_STICKY;
#endif
			category->drag_lock = ret ?
				enabled : LIBINPUT_CONFIG_DRAG_LOCK_DISABLED;
	} else if (!strcasecmp(key, "threeFingerDrag")) {
#if HAVE_LIBINPUT_CONFIG_3FG_DRAG_ENABLED_3FG
		if (!strcmp(content, "3")) {
			category->three_finger_drag =
				LIBINPUT_CONFIG_3FG_DRAG_ENABLED_3FG;
		} else if (!strcmp(content, "4")) {
			category->three_finger_drag =
				LIBINPUT_CONFIG_3FG_DRAG_ENABLED_4FG;
		} else {
			int ret = parse_bool(content, -1);
			if (ret < 0) {
				continue;
			}
			category->three_finger_drag = ret
				? LIBINPUT_CONFIG_3FG_DRAG_ENABLED_3FG
				: LIBINPUT_CONFIG_3FG_DRAG_DISABLED;
		}
#else
		wlr_log(WLR_ERROR, "<threeFingerDrag> is only"
			" supported in libinput >= 1.28");
#endif
		} else if (!strcasecmp(key, "accelProfile")) {
			category->accel_profile =
				get_accel_profile(content);
		} else if (!strcasecmp(key, "middleEmulation")) {
			int ret = parse_bool(content, -1);
			if (ret < 0) {
				continue;
			}
			category->middle_emu = ret
				? LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED
				: LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED;
		} else if (!strcasecmp(key, "disableWhileTyping")) {
			int ret = parse_bool(content, -1);
			if (ret < 0) {
				continue;
			}
			category->dwt = ret
				? LIBINPUT_CONFIG_DWT_ENABLED
				: LIBINPUT_CONFIG_DWT_DISABLED;
		} else if (!strcasecmp(key, "clickMethod")) {
			if (!strcasecmp(content, "none")) {
				category->click_method =
					LIBINPUT_CONFIG_CLICK_METHOD_NONE;
			} else if (!strcasecmp(content, "clickfinger")) {
				category->click_method =
					LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;
			} else if (!strcasecmp(content, "buttonAreas")) {
				category->click_method =
					LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
			} else {
				wlr_log(WLR_ERROR, "invalid clickMethod");
			}
		} else if (!strcasecmp(key, "scrollMethod")) {
			if (!strcasecmp(content, "none")) {
				category->scroll_method =
					LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
			} else if (!strcasecmp(content, "edge")) {
				category->scroll_method =
					LIBINPUT_CONFIG_SCROLL_EDGE;
			} else if (!strcasecmp(content, "twofinger")) {
				category->scroll_method =
					LIBINPUT_CONFIG_SCROLL_2FG;
			} else {
				wlr_log(WLR_ERROR, "invalid scrollMethod");
			}
		} else if (!strcasecmp(key, "sendEventsMode")) {
			category->send_events_mode =
				get_send_events_mode(content);
		} else if (!strcasecmp(key, "calibrationMatrix")) {
			errno = 0;
			category->have_calibration_matrix = true;
			float *mat = category->calibration_matrix;
			gchar **elements = g_strsplit(content, " ", -1);
			guint i = 0;
			for (; elements[i]; ++i) {
				char *end_str = NULL;
				mat[i] = strtof(elements[i], &end_str);
				if (errno == ERANGE || *end_str != '\0' || i == 6
						|| *elements[i] == '\0') {
					wlr_log(WLR_ERROR, "invalid calibration "
						"matrix element %s (index %d), "
						"expect six floats", elements[i], i);
					category->have_calibration_matrix = false;
					errno = 0;
					break;
				}
			}
			if (i != 6 && category->have_calibration_matrix) {
				wlr_log(WLR_ERROR, "wrong number of calibration "
					"matrix elements, expected 6, got %d", i);
				category->have_calibration_matrix = false;
			}
			g_strfreev(elements);
		} else if (!strcasecmp(key, "scrollFactor")) {
			set_double(content, &category->scroll_factor);
		}
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
			font->slant = PANGO_STYLE_ITALIC;
		} else if (!strcasecmp(content, "oblique")) {
			font->slant = PANGO_STYLE_OBLIQUE;
		} else {
			font->slant = PANGO_STYLE_NORMAL;
		}
	} else if (!strcmp(nodename, "weight")) {
		if (!strcasecmp(content, "thin")) {
			font->weight = PANGO_WEIGHT_THIN;
		} else if (!strcasecmp(content, "ultralight")) {
			font->weight = PANGO_WEIGHT_ULTRALIGHT;
		} else if (!strcasecmp(content, "light")) {
			font->weight = PANGO_WEIGHT_LIGHT;
		} else if (!strcasecmp(content, "semilight")) {
			font->weight = PANGO_WEIGHT_SEMILIGHT;
		} else if (!strcasecmp(content, "book")) {
			font->weight = PANGO_WEIGHT_BOOK;
		} else if (!strcasecmp(content, "medium")) {
			font->weight = PANGO_WEIGHT_MEDIUM;
		} else if (!strcasecmp(content, "semibold")) {
			font->weight = PANGO_WEIGHT_SEMIBOLD;
		} else if (!strcasecmp(content, "bold")) {
			font->weight = PANGO_WEIGHT_BOLD;
		} else if (!strcasecmp(content, "ultrabold")) {
			font->weight = PANGO_WEIGHT_ULTRABOLD;
		} else if (!strcasecmp(content, "heavy")) {
			font->weight = PANGO_WEIGHT_HEAVY;
		} else if (!strcasecmp(content, "ultraheavy")) {
			font->weight = PANGO_WEIGHT_ULTRAHEAVY;
		} else {
			font->weight = PANGO_WEIGHT_NORMAL;
		}
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
fill_font(xmlNode *node)
{
	enum font_place font_place = FONT_PLACE_NONE;
	char buf[256];
	if (lab_xml_get_string(node, "place", buf, sizeof(buf))) {
		font_place = enum_font_place(buf);
	}

	xmlNode *child;
	char *key, *content;
	LAB_XML_FOR_EACH(node, child, key, content) {
		switch (font_place) {
		case FONT_PLACE_NONE:
			/*
			 * If <theme><font></font></theme> is used without a
			 * place="" attribute, we set all font variables
			 */
			set_font_attr(&rc.font_activewindow, key, content);
			set_font_attr(&rc.font_inactivewindow, key, content);
			set_font_attr(&rc.font_menuheader, key, content);
			set_font_attr(&rc.font_menuitem, key, content);
			set_font_attr(&rc.font_osd, key, content);
			break;
		case FONT_PLACE_ACTIVEWINDOW:
			set_font_attr(&rc.font_activewindow, key, content);
			break;
		case FONT_PLACE_INACTIVEWINDOW:
			set_font_attr(&rc.font_inactivewindow, key, content);
			break;
		case FONT_PLACE_MENUHEADER:
			set_font_attr(&rc.font_menuheader, key, content);
			break;
		case FONT_PLACE_MENUITEM:
			set_font_attr(&rc.font_menuitem, key, content);
			break;
		case FONT_PLACE_OSD:
			set_font_attr(&rc.font_osd, key, content);
			break;

			/* TODO: implement for all font places */

		default:
			break;
		}
	}
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

/* Returns true if the node's children should also be traversed */
static bool
entry(xmlNode *node, char *nodename, char *content)
{
	string_truncate_at_pattern(nodename, ".openbox_config");
	string_truncate_at_pattern(nodename, ".labwc_config");

	if (getenv("LABWC_DEBUG_CONFIG_NODENAMES")) {
		printf("%s: %s\n", nodename, content);
	}

	/* handle nested nodes */
	if (!strcasecmp(nodename, "margin")) {
		fill_usable_area_override(node);
	} else if (!strcasecmp(nodename, "keybind.keyboard")) {
		fill_keybind(node);
	} else if (!strcasecmp(nodename, "context.mouse")) {
		fill_mouse_context(node);
	} else if (!strcasecmp(nodename, "touch")) {
		fill_touch(node);
	} else if (!strcasecmp(nodename, "device.libinput")) {
		fill_libinput_category(node);
	} else if (!strcasecmp(nodename, "regions")) {
		fill_regions(node);
	} else if (!strcasecmp(nodename, "fields.windowSwitcher")) {
		fill_window_switcher_fields(node);
	} else if (!strcasecmp(nodename, "windowRules")) {
		fill_window_rules(node);
	} else if (!strcasecmp(nodename, "font.theme")) {
		fill_font(node);
	} else if (!strcasecmp(nodename, "map.tablet")) {
		fill_tablet_button_map(node);

	/* handle nodes without content, e.g. <keyboard><default /> */
	} else if (!strcmp(nodename, "default.keyboard")) {
		load_default_key_bindings();
	} else if (!strcmp(nodename, "default.mouse")) {
		load_default_mouse_bindings();
	} else if (!strcasecmp(nodename, "prefix.desktops")) {
		xstrdup_replace(rc.workspace_config.prefix, content);
	} else if (!strcasecmp(nodename, "thumbnailLabelFormat.osd.windowSwitcher")) {
		xstrdup_replace(rc.window_switcher.thumbnail_label_format, content);

	} else if (!lab_xml_node_is_leaf(node)) {
		/* parse children of nested nodes other than above */
		return true;

	} else if (str_space_only(content)) {
		wlr_log(WLR_ERROR, "Empty string is not allowed for %s. "
			"Ignoring.", nodename);

	/* handle non-empty leaf nodes */
	} else if (!strcmp(nodename, "decoration.core")) {
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
	} else if (!strcasecmp(nodename, "xwaylandPersistence.core")) {
		set_bool(content, &rc.xwayland_persistence);
	} else if (!strcasecmp(nodename, "primarySelection.core")) {
		set_bool(content, &rc.primary_selection);

	} else if (!strcasecmp(nodename, "promptCommand.core")) {
		xstrdup_replace(rc.prompt_command, content);

	} else if (!strcmp(nodename, "policy.placement")) {
		enum lab_placement_policy policy = view_placement_parse(content);
		if (policy != LAB_PLACE_INVALID) {
			rc.placement_policy = policy;
		}
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
	} else if (!strcasecmp(nodename, "maximizedDecoration.theme")) {
		if (!strcasecmp(content, "titlebar")) {
			rc.hide_maximized_window_titlebar = false;
		} else if (!strcasecmp(content, "none")) {
			rc.hide_maximized_window_titlebar = true;
		}
	} else if (!strcasecmp(nodename, "dropShadows.theme")) {
		set_bool(content, &rc.shadows_enabled);
	} else if (!strcasecmp(nodename, "dropShadowsOnTiled.theme")) {
		set_bool(content, &rc.shadows_on_tiled);
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
		rc.snap_edge_range_inner = atoi(content);
		rc.snap_edge_range_outer = atoi(content);
		wlr_log(WLR_ERROR, "<snapping><range> is deprecated. "
			"Use <snapping><range inner=\"\" outer=\"\"> instead.");
	} else if (!strcasecmp(nodename, "inner.range.snapping")) {
		rc.snap_edge_range_inner = atoi(content);
	} else if (!strcasecmp(nodename, "outer.range.snapping")) {
		rc.snap_edge_range_outer = atoi(content);
	} else if (!strcasecmp(nodename, "cornerRange.snapping")) {
		rc.snap_edge_corner_range = atoi(content);
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

	/*
	 * <windowSwitcher preview="" outlines="">
	 *   <osd show="" style="" output="" thumbnailLabelFormat="" />
	 * </windowSwitcher>
	 *
	 * thumnailLabelFormat is handled above to allow for an empty value
	 */
	} else if (!strcasecmp(nodename, "show.osd.windowSwitcher")) {
		set_bool(content, &rc.window_switcher.show);
	} else if (!strcasecmp(nodename, "style.osd.windowSwitcher")) {
		if (!strcasecmp(content, "classic")) {
			rc.window_switcher.style = CYCLE_OSD_STYLE_CLASSIC;
		} else if (!strcasecmp(content, "thumbnail")) {
			rc.window_switcher.style = CYCLE_OSD_STYLE_THUMBNAIL;
		} else {
			wlr_log(WLR_ERROR, "Invalid windowSwitcher style %s: "
				"should be one of classic|thumbnail", content);
		}
	} else if (!strcasecmp(nodename, "output.osd.windowSwitcher")) {
		if (!strcasecmp(content, "all")) {
			rc.window_switcher.output_criteria = CYCLE_OSD_OUTPUT_ALL;
		} else if (!strcasecmp(content, "cursor")) {
			rc.window_switcher.output_criteria = CYCLE_OSD_OUTPUT_CURSOR;
		} else if (!strcasecmp(content, "focused")) {
			rc.window_switcher.output_criteria = CYCLE_OSD_OUTPUT_FOCUSED;
		} else {
			wlr_log(WLR_ERROR, "Invalid windowSwitcher output %s: "
				"should be one of all|focused|cursor", content);
		}

	/* The following two are for backward compatibility only. */
	} else if (!strcasecmp(nodename, "show.windowSwitcher")) {
		set_bool(content, &rc.window_switcher.show);
		wlr_log(WLR_ERROR, "<windowSwitcher show=\"\" /> is deprecated."
			" Use <windowSwitcher><osd show=\"\" />");
	} else if (!strcasecmp(nodename, "style.windowSwitcher")) {
		if (!strcasecmp(content, "classic")) {
			rc.window_switcher.style = CYCLE_OSD_STYLE_CLASSIC;
		} else if (!strcasecmp(content, "thumbnail")) {
			rc.window_switcher.style = CYCLE_OSD_STYLE_THUMBNAIL;
		}
		wlr_log(WLR_ERROR, "<windowSwitcher style=\"\" /> is deprecated."
			" Use <windowSwitcher><osd style=\"\" />");

	} else if (!strcasecmp(nodename, "preview.windowSwitcher")) {
		set_bool(content, &rc.window_switcher.preview);
	} else if (!strcasecmp(nodename, "outlines.windowSwitcher")) {
		set_bool(content, &rc.window_switcher.outlines);
	} else if (!strcasecmp(nodename, "allWorkspaces.windowSwitcher")) {
		if (parse_bool(content, -1) == true) {
			rc.window_switcher.criteria &=
				~LAB_VIEW_CRITERIA_CURRENT_WORKSPACE;
		}
	} else if (!strcasecmp(nodename, "unshade.windowSwitcher")) {
		set_bool(content, &rc.window_switcher.unshade);

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
	} else if (!strcasecmp(nodename, "initial.desktops")) {
		xstrdup_replace(rc.workspace_config.initial_workspace_name, content);
	} else if (!strcasecmp(nodename, "number.desktops")) {
		rc.workspace_config.min_nr_workspaces = MAX(1, atoi(content));
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
		rc.mag_scale = MAX(1.0, rc.mag_scale);
	} else if (!strcasecmp(nodename, "increment.magnifier")) {
		set_float(content, &rc.mag_increment);
		rc.mag_increment = MAX(0, rc.mag_increment);
	} else if (!strcasecmp(nodename, "useFilter.magnifier")) {
		set_bool(content, &rc.mag_filter);
	}

	return false;
}

static void
traverse(xmlNode *node)
{
	xmlNode *child;
	char *key, *content;
	LAB_XML_FOR_EACH(node, child, key, content) {
		(void)key;
		char buffer[256];
		char *name = nodename(child, buffer, sizeof(buffer));
		if (entry(child, name, content)) {
			traverse(child);
		}
	}
}

static void
rcxml_parse_xml(struct buf *b)
{
	int options = 0;
	xmlDoc *d = xmlReadMemory(b->data, b->len, NULL, NULL, options);
	if (!d) {
		wlr_log(WLR_ERROR, "error parsing config file");
		return;
	}
	xmlNode *root = xmlDocGetRootElement(d);

	lab_xml_expand_dotted_attributes(root);
	traverse(root);

	xmlFreeDoc(d);
	xmlCleanupParser();
}

static void
init_font_defaults(struct font *font)
{
	font->size = 10;
	font->slant = PANGO_STYLE_NORMAL;
	font->weight = PANGO_WEIGHT_NORMAL;
}

static void
rcxml_init(void)
{
	static bool has_run;

	if (!has_run) {
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
	rc.hide_maximized_window_titlebar = false;
	rc.show_title = true;
	rc.title_layout_loaded = false;
	rc.ssd_keep_border = true;
	rc.corner_radius = 8;
	rc.shadows_enabled = false;
	rc.shadows_on_tiled = false;

	rc.gap = 0;
	rc.adaptive_sync = LAB_ADAPTIVE_SYNC_DISABLED;
	rc.allow_tearing = LAB_TEARING_DISABLED;
	rc.auto_enable_outputs = true;
	rc.reuse_output_mode = false;
	rc.xwayland_persistence = false;
	rc.primary_selection = true;

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
	rc.tablet.rotation = LAB_ROTATE_NONE;
	rc.tablet.box = (struct wlr_fbox){0};
	tablet_load_default_button_mappings();
	rc.tablet_tool.motion = LAB_MOTION_ABSOLUTE;
	rc.tablet_tool.relative_motion_sensitivity = 1.0;

	rc.repeat_rate = 25;
	rc.repeat_delay = 600;
	rc.kb_numlock_enable = LAB_STATE_UNSPECIFIED;
	rc.kb_layout_per_window = false;
	rc.screen_edge_strength = 20;
	rc.window_edge_strength = 20;
	rc.unsnap_threshold = 20;
	rc.unmaximize_threshold = 150;

	rc.snap_edge_range_inner = 10;
	rc.snap_edge_range_outer = 10;
	rc.snap_edge_corner_range = 50;
	rc.snap_overlay_enabled = true;
	rc.snap_overlay_delay_inner = 500;
	rc.snap_overlay_delay_outer = 500;
	rc.snap_top_maximize = true;
	rc.snap_tiling_events_mode = LAB_TILING_EVENTS_ALWAYS;

	rc.window_switcher.show = true;
	rc.window_switcher.style = CYCLE_OSD_STYLE_CLASSIC;
	rc.window_switcher.output_criteria = CYCLE_OSD_OUTPUT_ALL;
	rc.window_switcher.thumbnail_label_format = xstrdup("%T");
	rc.window_switcher.preview = true;
	rc.window_switcher.outlines = true;
	rc.window_switcher.unshade = true;
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

static void
load_default_window_switcher_fields(void)
{
	static const struct {
		enum cycle_osd_field_content content;
		int width;
	} fields[] = {
#if HAVE_LIBSFDO
		{ LAB_FIELD_ICON, 5 },
		{ LAB_FIELD_DESKTOP_ENTRY_NAME, 30 },
		{ LAB_FIELD_TITLE, 65 },
#else
		{ LAB_FIELD_DESKTOP_ENTRY_NAME, 30 },
		{ LAB_FIELD_TITLE, 70 },
#endif
	};

	for (size_t i = 0; i < ARRAY_SIZE(fields); i++) {
		struct cycle_osd_field *field = znew(*field);
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

	if (!rc.prompt_command) {
		rc.prompt_command =
			xstrdup("labnag "
				"--message '%m' "
				"--button-dismiss '%n' "
				"--button-dismiss '%y' "
				"--background-color '%b' "
				"--text-color '%t' "
				"--button-border-color '%t' "
				"--border-bottom-color '%t' "
				"--button-background-color '%b' "
				"--button-text-color '%t' "
				"--border-bottom-size 1 "
				"--button-border-size 3 "
				"--keyboard-focus on-demand "
				"--layer overlay "
				"--timeout 0");
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

		struct buf b = BUF_INIT;
		struct workspace *workspace;
		for (int i = nr_workspaces; i < rc.workspace_config.min_nr_workspaces; i++) {
			workspace = znew(*workspace);
			if (!string_null_or_empty(rc.workspace_config.prefix)) {
				buf_add_fmt(&b, "%s ", rc.workspace_config.prefix);
			}
			buf_add_fmt(&b, "%d", i + 1);
			workspace->name = xstrdup(b.data);
			wl_list_append(&rc.workspace_config.workspaces, &workspace->link);
			buf_clear(&b);
		}
		buf_reset(&b);
	}
	if (rc.workspace_config.popuptime == INT_MIN) {
		rc.workspace_config.popuptime = 1000;
	}
	if (!wl_list_length(&rc.window_switcher.fields)) {
		wlr_log(WLR_INFO, "load default window switcher fields");
		load_default_window_switcher_fields();
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
	int field_width_sum = 0;
	struct cycle_osd_field *field, *field_tmp;
	wl_list_for_each_safe(field, field_tmp, &rc.window_switcher.fields, link) {
		field_width_sum += field->width;
		if (!cycle_osd_field_is_valid(field) || field_width_sum > 100) {
			wlr_log(WLR_ERROR, "Deleting invalid window switcher field %p", field);
			wl_list_remove(&field->link);
			cycle_osd_field_free(field);
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
		struct buf b = buf_from_file(path->string);
		if (!b.len) {
			continue;
		}

		wlr_log(WLR_INFO, "read config file %s", path->string);

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
	zfree(rc.prompt_command);
	zfree(rc.theme_name);
	zfree(rc.icon_theme_name);
	zfree(rc.fallback_app_icon_name);
	zfree(rc.workspace_config.prefix);
	zfree(rc.workspace_config.initial_workspace_name);
	zfree(rc.tablet.output_name);
	zfree(rc.window_switcher.thumbnail_label_format);

	clear_title_layout();

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

	clear_window_switcher_fields();

	struct window_rule *rule, *rule_tmp;
	wl_list_for_each_safe(rule, rule_tmp, &rc.window_rules, link) {
		rule_destroy(rule);
	}

	/* Reset state vars for starting fresh when Reload is triggered */
	mouse_scroll_factor = -1;
}
