// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <fcntl.h>
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
#include "action.h"
#include "common/dir.h"
#include "common/list.h"
#include "common/macros.h"
#include "common/mem.h"
#include "common/nodename.h"
#include "common/parse-bool.h"
#include "common/string-helpers.h"
#include "config/keybind.h"
#include "config/libinput.h"
#include "config/mousebind.h"
#include "config/tablet.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "regions.h"
#include "view.h"
#include "window-rules.h"
#include "workspaces.h"
#include "theme.h"

static bool in_regions;
static bool in_usable_area_override;
static bool in_keybind;
static bool in_mousebind;
static bool in_touch;
static bool in_libinput_category;
static bool in_window_switcher_field;
static bool in_window_rules;
static bool in_action_query;
static bool in_action_then_branch;
static bool in_action_else_branch;

static struct usable_area_override *current_usable_area_override;
static struct keybind *current_keybind;
static struct mousebind *current_mousebind;
static struct touch_config_entry *current_touch;
static struct libinput_category *current_libinput_category;
static const char *current_mouse_context;
static struct action *current_keybind_action;
static struct action *current_mousebind_action;
static struct region *current_region;
static struct window_switcher_field *current_field;
static struct window_rule *current_window_rule;
static struct action *current_window_rule_action;
static struct view_query *current_view_query;
static struct action *current_child_action;

enum font_place {
	FONT_PLACE_NONE = 0,
	FONT_PLACE_UNKNOWN,
	FONT_PLACE_ACTIVEWINDOW,
	FONT_PLACE_INACTIVEWINDOW,
	FONT_PLACE_MENUITEM,
	FONT_PLACE_OSD,
	/* TODO: Add all places based on Openbox's rc.xml */
};

static void load_default_key_bindings(void);
static void load_default_mouse_bindings(void);

static void
fill_usable_area_override(char *nodename, char *content)
{
	if (!strcasecmp(nodename, "margin")) {
		current_usable_area_override = znew(*current_usable_area_override);
		wl_list_append(&rc.usable_area_overrides, &current_usable_area_override->link);
		return;
	}
	string_truncate_at_pattern(nodename, ".margin");
	if (!content) {
		/* nop */
	} else if (!current_usable_area_override) {
		wlr_log(WLR_ERROR, "no usable-area-override object");
	} else if (!strcmp(nodename, "output")) {
		free(current_usable_area_override->output);
		current_usable_area_override->output = xstrdup(content);
	} else if (!strcmp(nodename, "left")) {
		current_usable_area_override->margin.left = atoi(content);
	} else if (!strcmp(nodename, "right")) {
		current_usable_area_override->margin.right = atoi(content);
	} else if (!strcmp(nodename, "top")) {
		current_usable_area_override->margin.top = atoi(content);
	} else if (!strcmp(nodename, "bottom")) {
		current_usable_area_override->margin.bottom = atoi(content);
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
fill_window_rule(char *nodename, char *content)
{
	if (!strcasecmp(nodename, "windowRule.windowRules")) {
		current_window_rule = znew(*current_window_rule);
		wl_list_append(&rc.window_rules, &current_window_rule->link);
		wl_list_init(&current_window_rule->actions);
		return;
	}

	string_truncate_at_pattern(nodename, ".windowrule.windowrules");
	if (!content) {
		/* nop */
	} else if (!current_window_rule) {
		wlr_log(WLR_ERROR, "no window-rule");

	/* Criteria */
	} else if (!strcmp(nodename, "identifier")) {
		free(current_window_rule->identifier);
		current_window_rule->identifier = xstrdup(content);
		wlr_log(WLR_INFO, "Identifier found: %s=\"%s\"", nodename, content);
	} else if (!strcmp(nodename, "title")) {
		free(current_window_rule->title);
		current_window_rule->title = xstrdup(content);
	} else if (!strcasecmp(nodename, "matchOnce")) {
		set_bool(content, &current_window_rule->match_once);

	/* Event */
	} else if (!strcmp(nodename, "event")) {
		/*
		 * This is just in readiness for adding any other types of
		 * events in the future. We default to onFirstMap anyway.
		 */
		if (!strcasecmp(content, "onFirstMap")) {
			current_window_rule->event = LAB_WINDOW_RULE_EVENT_ON_FIRST_MAP;
		}

	/* Properties */
	} else if (!strcasecmp(nodename, "serverDecoration")) {
		set_property(content, &current_window_rule->server_decoration);
	} else if (!strcasecmp(nodename, "skipTaskbar")) {
		set_property(content, &current_window_rule->skip_taskbar);
	} else if (!strcasecmp(nodename, "skipWindowSwitcher")) {
		set_property(content, &current_window_rule->skip_window_switcher);
	} else if (!strcasecmp(nodename, "ignoreFocusRequest")) {
		set_property(content, &current_window_rule->ignore_focus_request);
	} else if (!strcasecmp(nodename, "fixedPosition")) {
		set_property(content, &current_window_rule->fixed_position);

	/* Custom border properties: color */
	} else if (!strcasecmp(nodename, "borderColor")) {
		theme_parse_hexstr(content, current_window_rule->custom_border_color);
		current_window_rule->has_custom_border = true;

	/* Actions */
	} else if (!strcmp(nodename, "name.action")) {
		current_window_rule_action = action_create(content);
		if (current_window_rule_action) {
			wl_list_append(&current_window_rule->actions,
				&current_window_rule_action->link);
		}
	} else if (!current_window_rule_action) {
		wlr_log(WLR_ERROR, "expect <action name=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
	} else {
		action_arg_from_xml_node(current_window_rule_action, nodename, content);
	}
}

static void
fill_window_switcher_field(char *nodename, char *content)
{
	if (!strcasecmp(nodename, "field.fields.windowswitcher")) {
		current_field = znew(*current_field);
		wl_list_append(&rc.window_switcher.fields, &current_field->link);
		return;
	}

	string_truncate_at_pattern(nodename, ".field.fields.windowswitcher");
	if (!content) {
		/* intentionally left empty */
	} else if (!current_field) {
		wlr_log(WLR_ERROR, "no <field>");
	} else if (!strcmp(nodename, "content")) {
		if (!strcmp(content, "type")) {
			current_field->content = LAB_FIELD_TYPE;
		} else if (!strcmp(content, "identifier")) {
			current_field->content = LAB_FIELD_IDENTIFIER;
		} else if (!strcmp(content, "app_id")) {
			wlr_log(WLR_ERROR, "window-switcher field 'app_id' is deprecated");
			current_field->content = LAB_FIELD_IDENTIFIER;
		} else if (!strcmp(content, "trimmed_identifier")) {
			current_field->content = LAB_FIELD_TRIMMED_IDENTIFIER;
		} else if (!strcmp(content, "title")) {
			current_field->content = LAB_FIELD_TITLE;
		} else {
			wlr_log(WLR_ERROR, "bad windowSwitcher field '%s'", content);
		}
	} else if (!strcmp(nodename, "width") && !strchr(content, '%')) {
		wlr_log(WLR_ERROR, "Removing invalid field, %s='%s' misses"
			" trailing %%", nodename, content);
		wl_list_remove(&current_field->link);
		zfree(current_field);
	} else if (!strcmp(nodename, "width")) {
		current_field->width = atoi(content);
	} else {
		wlr_log(WLR_ERROR, "Unexpected data in field parser: %s=\"%s\"",
			nodename, content);
	}
}

static void
fill_region(char *nodename, char *content)
{
	string_truncate_at_pattern(nodename, ".region.regions");

	if (!strcasecmp(nodename, "region.regions")) {
		current_region = znew(*current_region);
		wl_list_append(&rc.regions, &current_region->link);
	} else if (!content) {
		/* intentionally left empty */
	} else if (!current_region) {
		wlr_log(WLR_ERROR, "Expecting <region name=\"\" before %s='%s'",
			nodename, content);
	} else if (!strcasecmp(nodename, "name")) {
		/* Prevent leaking memory if config contains multiple names */
		if (!current_region->name) {
			current_region->name = xstrdup(content);
		}
	} else if (strstr("xywidtheight", nodename) && !strchr(content, '%')) {
		wlr_log(WLR_ERROR, "Removing invalid region '%s': %s='%s' misses"
			" a trailing %%", current_region->name, nodename, content);
		wl_list_remove(&current_region->link);
		zfree(current_region->name);
		zfree(current_region);
	} else if (!strcmp(nodename, "x")) {
		current_region->percentage.x = atoi(content);
	} else if (!strcmp(nodename, "y")) {
		current_region->percentage.y = atoi(content);
	} else if (!strcmp(nodename, "width")) {
		current_region->percentage.width = atoi(content);
	} else if (!strcmp(nodename, "height")) {
		current_region->percentage.height = atoi(content);
	} else {
		wlr_log(WLR_ERROR, "Unexpected data in region parser: %s=\"%s\"",
			nodename, content);
	}
}

static void
fill_action_query(char *nodename, char *content, struct action *action)
{
	string_truncate_at_pattern(nodename, ".keybind.keyboard");
	string_truncate_at_pattern(nodename, ".mousebind.context.mouse");

	if (!strcasecmp(nodename, "query.action")) {
		current_view_query = NULL;
	}

	string_truncate_at_pattern(nodename, ".query.action");

	if (!content) {
		return;
	}

	if (!current_view_query) {
		struct wl_list *queries = action_get_querylist(action, "query");
		if (!queries) {
			action_arg_add_querylist(action, "query");
			queries = action_get_querylist(action, "query");
		}
		current_view_query = znew(*current_view_query);
		wl_list_append(queries, &current_view_query->link);
	}

	if (!strcasecmp(nodename, "identifier")) {
		current_view_query->identifier = xstrdup(content);
	} else if (!strcasecmp(nodename, "title")) {
		current_view_query->title = xstrdup(content);
	}
}

static void
fill_child_action(char *nodename, char *content, struct action *parent,
	const char *branch_name)
{
	string_truncate_at_pattern(nodename, ".keybind.keyboard");
	string_truncate_at_pattern(nodename, ".mousebind.context.mouse");
	string_truncate_at_pattern(nodename, ".then.action");
	string_truncate_at_pattern(nodename, ".else.action");

	if (!strcasecmp(nodename, "action")) {
		current_child_action = NULL;
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
		current_child_action = action_create(content);
		if (current_child_action) {
			wl_list_append(siblings, &current_child_action->link);
		}
	} else if (!current_child_action) {
		wlr_log(WLR_ERROR, "expect <action name=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
	} else {
		action_arg_from_xml_node(current_child_action, nodename, content);
	}
}

static void
fill_keybind(char *nodename, char *content)
{
	if (!content) {
		return;
	}
	string_truncate_at_pattern(nodename, ".keybind.keyboard");
	if (!strcmp(nodename, "key")) {
		current_keybind = keybind_create(content);
		current_keybind_action = NULL;
		/*
		 * If an invalid keybind has been provided,
		 * keybind_create() complains.
		 */
		if (!current_keybind) {
			wlr_log(WLR_ERROR, "Invalid keybind: %s", content);
			return;
		}
	} else if (!current_keybind) {
		wlr_log(WLR_ERROR, "expect <keybind key=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
	} else if (!strcasecmp(nodename, "layoutDependent")) {
		set_bool(content, &current_keybind->use_syms_only);
	} else if (!strcmp(nodename, "name.action")) {
		current_keybind_action = action_create(content);
		if (current_keybind_action) {
			wl_list_append(&current_keybind->actions,
				&current_keybind_action->link);
		}
	} else if (!current_keybind_action) {
		wlr_log(WLR_ERROR, "expect <action name=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
	} else {
		/*
		 * Here we deal with action sub-elements such as <to>, <output>,
		 * <region>, <direction> and so on. This is common to key- and
		 * mousebinds.
		 */
		action_arg_from_xml_node(current_keybind_action, nodename, content);
	}
}

static void
fill_mousebind(char *nodename, char *content)
{
	/*
	 * Example of what we are parsing:
	 * <mousebind button="Left" action="DoubleClick">
	 *   <action name="Focus"/>
	 *   <action name="Raise"/>
	 *   <action name="ToggleMaximize"/>
	 * </mousebind>
	 */

	if (!current_mouse_context) {
		wlr_log(WLR_ERROR, "expect <context name=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
		return;
	} else if (!strcmp(nodename, "mousebind.context.mouse")) {
		wlr_log(WLR_INFO, "create mousebind for %s",
			current_mouse_context);
		current_mousebind = mousebind_create(current_mouse_context);
		current_mousebind_action = NULL;
		return;
	} else if (!content) {
		return;
	}

	string_truncate_at_pattern(nodename, ".mousebind.context.mouse");
	if (!current_mousebind) {
		wlr_log(WLR_ERROR,
			"expect <mousebind button=\"\" action=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
	} else if (!strcmp(nodename, "button")) {
		current_mousebind->button = mousebind_button_from_str(content,
			&current_mousebind->modifiers);
	} else if (!strcmp(nodename, "direction")) {
		current_mousebind->direction = mousebind_direction_from_str(content,
			&current_mousebind->modifiers);
	} else if (!strcmp(nodename, "action")) {
		/* <mousebind button="" action="EVENT"> */
		current_mousebind->mouse_event =
			mousebind_event_from_str(content);
	} else if (!strcmp(nodename, "name.action")) {
		current_mousebind_action = action_create(content);
		if (current_mousebind_action) {
			wl_list_append(&current_mousebind->actions,
				&current_mousebind_action->link);
		}
	} else if (!current_mousebind_action) {
		wlr_log(WLR_ERROR, "expect <action name=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
	} else {
		action_arg_from_xml_node(current_mousebind_action, nodename, content);
	}
}

static void
fill_touch(char *nodename, char *content)
{
	if (!strcasecmp(nodename, "touch")) {
		current_touch = znew(*current_touch);
		wl_list_append(&rc.touch_configs, &current_touch->link);
	} else if (!strcasecmp(nodename, "deviceName.touch")) {
		current_touch->device_name = xstrdup(content);
	} else if (!strcasecmp(nodename, "mapToOutput.touch")) {
		current_touch->output_name = xstrdup(content);
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
fill_libinput_category(char *nodename, char *content)
{
	/*
	 * Create a new profile (libinput-category) on `<libinput><device>`
	 * so that the 'default' profile can be created without even providing a
	 * category="" attribute (same as <device category="default">...)
	 */
	if (!strcmp(nodename, "device.libinput")) {
		current_libinput_category = libinput_category_create();
	}

	if (!content) {
		return;
	}

	if (!current_libinput_category) {
		return;
	}

	string_truncate_at_pattern(nodename, ".device.libinput");

	if (!strcmp(nodename, "category")) {
		/*
		 * First we try to get a type based on a number of pre-defined
		 * terms, for example: 'default', 'touch', 'touchpad' and
		 * 'non-touch'
		 */
		current_libinput_category->type = get_device_type(content);

		/*
		 * If we couldn't match against any of those terms, we use the
		 * provided value to define the device name that the settings
		 * should be applicable to.
		 */
		if (current_libinput_category->type == LAB_LIBINPUT_DEVICE_NONE) {
			current_libinput_category->name = xstrdup(content);
		}
	} else if (!strcasecmp(nodename, "naturalScroll")) {
		set_bool_as_int(content, &current_libinput_category->natural_scroll);
	} else if (!strcasecmp(nodename, "leftHanded")) {
		set_bool_as_int(content, &current_libinput_category->left_handed);
	} else if (!strcasecmp(nodename, "pointerSpeed")) {
		current_libinput_category->pointer_speed = atof(content);
		if (current_libinput_category->pointer_speed < -1) {
			current_libinput_category->pointer_speed = -1;
		} else if (current_libinput_category->pointer_speed > 1) {
			current_libinput_category->pointer_speed = 1;
		}
	} else if (!strcasecmp(nodename, "tap")) {
		int ret = parse_bool(content, -1);
		if (ret < 0) {
			return;
		}
		current_libinput_category->tap = ret
			? LIBINPUT_CONFIG_TAP_ENABLED
			: LIBINPUT_CONFIG_TAP_DISABLED;
	} else if (!strcasecmp(nodename, "tapButtonMap")) {
		if (!strcmp(content, "lrm")) {
			current_libinput_category->tap_button_map =
				LIBINPUT_CONFIG_TAP_MAP_LRM;
		} else if (!strcmp(content, "lmr")) {
			current_libinput_category->tap_button_map =
				LIBINPUT_CONFIG_TAP_MAP_LMR;
		} else {
			wlr_log(WLR_ERROR, "invalid tapButtonMap");
		}
	} else if (!strcasecmp(nodename, "tapAndDrag")) {
		int ret = parse_bool(content, -1);
		if (ret < 0) {
			return;
		}
		current_libinput_category->tap_and_drag = ret
			? LIBINPUT_CONFIG_DRAG_ENABLED
			: LIBINPUT_CONFIG_DRAG_DISABLED;
	} else if (!strcasecmp(nodename, "dragLock")) {
		int ret = parse_bool(content, -1);
		if (ret < 0) {
			return;
		}
		current_libinput_category->drag_lock = ret
			? LIBINPUT_CONFIG_DRAG_LOCK_ENABLED
			: LIBINPUT_CONFIG_DRAG_LOCK_DISABLED;
	} else if (!strcasecmp(nodename, "accelProfile")) {
		current_libinput_category->accel_profile =
			get_accel_profile(content);
	} else if (!strcasecmp(nodename, "middleEmulation")) {
		int ret = parse_bool(content, -1);
		if (ret < 0) {
			return;
		}
		current_libinput_category->middle_emu = ret
			? LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED
			: LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED;
	} else if (!strcasecmp(nodename, "disableWhileTyping")) {
		int ret = parse_bool(content, -1);
		if (ret < 0) {
			return;
		}
		current_libinput_category->dwt = ret
			? LIBINPUT_CONFIG_DWT_ENABLED
			: LIBINPUT_CONFIG_DWT_DISABLED;
	} else if (!strcasecmp(nodename, "clickMethod")) {
		if (!strcasecmp(content, "none")) {
			current_libinput_category->click_method =
				LIBINPUT_CONFIG_CLICK_METHOD_NONE;
		} else if (!strcasecmp(content, "clickfinger")) {
			current_libinput_category->click_method =
				LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;
		} else if (!strcasecmp(content, "buttonAreas")) {
			current_libinput_category->click_method =
				LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
		} else {
			wlr_log(WLR_ERROR, "invalid clickMethod");
		}
	} else if (!strcasecmp(nodename, "sendEventsMode")) {
		current_libinput_category->send_events_mode =
			get_send_events_mode(content);
	}
}

static void
set_font_attr(struct font *font, const char *nodename, const char *content)
{
	if (!strcmp(nodename, "name")) {
		zfree(font->name);
		font->name = xstrdup(content);
	} else if (!strcmp(nodename, "size")) {
		font->size = atoi(content);
	} else if (!strcmp(nodename, "slant")) {
		font->slant = !strcasecmp(content, "italic") ?
			FONT_SLANT_ITALIC : FONT_SLANT_NORMAL;
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
		set_font_attr(&rc.font_menuitem, nodename, content);
		set_font_attr(&rc.font_osd, nodename, content);
		break;
	case FONT_PLACE_ACTIVEWINDOW:
		set_font_attr(&rc.font_activewindow, nodename, content);
		break;
	case FONT_PLACE_INACTIVEWINDOW:
		set_font_attr(&rc.font_inactivewindow, nodename, content);
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
entry(xmlNode *node, char *nodename, char *content)
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

	if (in_usable_area_override) {
		fill_usable_area_override(nodename, content);
	}
	if (in_keybind) {
		if (in_action_query) {
			fill_action_query(nodename, content,
				current_keybind_action);
		} else if (in_action_then_branch) {
			fill_child_action(nodename, content,
				current_keybind_action, "then");
		} else if (in_action_else_branch) {
			fill_child_action(nodename, content,
				current_keybind_action, "else");
		} else {
			fill_keybind(nodename, content);
		}
	}
	if (in_mousebind) {
		if (in_action_query) {
			fill_action_query(nodename, content,
				current_mousebind_action);
		} else if (in_action_then_branch) {
			fill_child_action(nodename, content,
				current_mousebind_action, "then");
		} else if (in_action_else_branch) {
			fill_child_action(nodename, content,
				current_mousebind_action, "else");
		} else {
			fill_mousebind(nodename, content);
		}
	}
	if (in_touch) {
		fill_touch(nodename, content);
		return;
	}
	if (in_libinput_category) {
		fill_libinput_category(nodename, content);
		return;
	}
	if (in_regions) {
		fill_region(nodename, content);
		return;
	}
	if (in_window_switcher_field) {
		fill_window_switcher_field(nodename, content);
		return;
	}
	if (in_window_rules) {
		fill_window_rule(nodename, content);
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
		set_bool(content, &rc.allow_tearing);
		if (rc.allow_tearing) {
			char *no_atomic_env = getenv("WLR_DRM_NO_ATOMIC");
			if (!no_atomic_env || strcmp(no_atomic_env, "1") != 0) {
				rc.allow_tearing = false;
				wlr_log(WLR_ERROR, "tearing requires WLR_DRM_NO_ATOMIC=1");
			}
		}
	} else if (!strcasecmp(nodename, "reuseOutputMode.core")) {
		set_bool(content, &rc.reuse_output_mode);
	} else if (!strcmp(nodename, "policy.placement")) {
		if (!strcmp(content, "automatic")) {
			rc.placement_policy = LAB_PLACE_AUTOMATIC;
		} else if (!strcmp(content, "cursor")) {
			rc.placement_policy = LAB_PLACE_CURSOR;
		} else {
			rc.placement_policy = LAB_PLACE_CENTER;
		}
	} else if (!strcmp(nodename, "name.theme")) {
		rc.theme_name = xstrdup(content);
	} else if (!strcmp(nodename, "cornerradius.theme")) {
		rc.corner_radius = atoi(content);
	} else if (!strcasecmp(nodename, "keepBorder.theme")) {
		set_bool(content, &rc.ssd_keep_border);
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
		rc.scroll_factor = atof(content);
	} else if (!strcasecmp(nodename, "name.context.mouse")) {
		current_mouse_context = content;
		current_mousebind = NULL;

	} else if (!strcasecmp(nodename, "repeatRate.keyboard")) {
		rc.repeat_rate = atoi(content);
	} else if (!strcasecmp(nodename, "repeatDelay.keyboard")) {
		rc.repeat_delay = atoi(content);
	} else if (!strcasecmp(nodename, "numlock.keyboard")) {
		set_bool(content, &rc.kb_numlock_enable);
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
	} else if (!strcasecmp(nodename, "range.snapping")) {
		rc.snap_edge_range = atoi(content);
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
	} else if (!strcasecmp(nodename, "mapToOutput.tablet")) {
		rc.tablet.output_name = xstrdup(content);
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
	}
}

static void
process_node(xmlNode *node)
{
	char *content;
	static char buffer[256];
	char *name;

	content = (char *)node->content;
	if (xmlIsBlankNode(node)) {
		return;
	}
	name = nodename(node, buffer, sizeof(buffer));
	entry(node, name, content);
}

static void xml_tree_walk(xmlNode *node);

static void
traverse(xmlNode *n)
{
	xmlAttr *attr;

	process_node(n);
	for (attr = n->properties; attr; attr = attr->next) {
		xml_tree_walk(attr->children);
	}
	xml_tree_walk(n->children);
}

static void
xml_tree_walk(xmlNode *node)
{
	for (xmlNode *n = node; n && n->name; n = n->next) {
		if (!strcasecmp((char *)n->name, "comment")) {
			continue;
		}
		if (!strcasecmp((char *)n->name, "margin")) {
			in_usable_area_override = true;
			traverse(n);
			in_usable_area_override = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "keybind")) {
			in_keybind = true;
			traverse(n);
			in_keybind = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "mousebind")) {
			in_mousebind = true;
			traverse(n);
			in_mousebind = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "touch")) {
			in_touch = true;
			traverse(n);
			in_touch = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "device")) {
			in_libinput_category = true;
			traverse(n);
			in_libinput_category = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "regions")) {
			in_regions = true;
			traverse(n);
			in_regions = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "fields")) {
			in_window_switcher_field = true;
			traverse(n);
			in_window_switcher_field = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "windowRules")) {
			in_window_rules = true;
			traverse(n);
			in_window_rules = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "query")) {
			in_action_query = true;
			traverse(n);
			in_action_query = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "then")) {
			in_action_then_branch = true;
			traverse(n);
			in_action_then_branch = false;
			continue;
		}
		if (!strcasecmp((char *)n->name, "else")) {
			in_action_else_branch = true;
			traverse(n);
			in_action_else_branch = false;
			continue;
		}
		traverse(n);
	}
}

/* Exposed in header file to allow unit tests to parse buffers */
void
rcxml_parse_xml(struct buf *b)
{
	xmlDoc *d = xmlParseMemory(b->buf, b->len);
	if (!d) {
		wlr_log(WLR_ERROR, "error parsing config file");
		return;
	}
	xml_tree_walk(xmlDocGetRootElement(d));
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

	rc.placement_policy = LAB_PLACE_CENTER;

	rc.xdg_shell_server_side_deco = true;
	rc.ssd_keep_border = true;
	rc.corner_radius = 8;

	init_font_defaults(&rc.font_activewindow);
	init_font_defaults(&rc.font_inactivewindow);
	init_font_defaults(&rc.font_menuitem);
	init_font_defaults(&rc.font_osd);

	rc.focus_follow_mouse = false;
	rc.focus_follow_mouse_requires_movement = true;
	rc.raise_on_focus = false;

	rc.doubleclick_time = 500;
	rc.scroll_factor = 1.0;

	rc.tablet.output_name = NULL;
	rc.tablet.rotation = 0;
	rc.tablet.box = (struct wlr_fbox){0};
	tablet_load_default_button_mappings();

	rc.repeat_rate = 25;
	rc.repeat_delay = 600;
	rc.kb_numlock_enable = true;
	rc.kb_layout_per_window = false;
	rc.screen_edge_strength = 20;
	rc.window_edge_strength = 20;

	rc.snap_edge_range = 1;
	rc.snap_top_maximize = true;
	rc.snap_tiling_events_mode = LAB_TILING_EVENTS_ALWAYS;

	rc.window_switcher.show = true;
	rc.window_switcher.preview = true;
	rc.window_switcher.outlines = true;

	rc.resize_indicator = LAB_RESIZE_INDICATOR_NEVER;

	rc.workspace_config.popuptime = INT_MIN;
	rc.workspace_config.min_nr_workspaces = 1;
}

static struct {
	const char *binding, *action, *attribute, *value;
} key_combos[] = {
	{ "A-Tab", "NextWindow", NULL, NULL },
	{ "W-Return", "Execute", "command", "alacritty" },
	{ "A-F3", "Execute", "command", "bemenu-run" },
	{ "A-F4", "Close", NULL, NULL },
	{ "W-a", "ToggleMaximize", NULL, NULL },
	{ "A-Left", "MoveToEdge", "direction", "left" },
	{ "A-Right", "MoveToEdge", "direction", "right" },
	{ "A-Up", "MoveToEdge", "direction", "up" },
	{ "A-Down", "MoveToEdge", "direction", "down" },
	{ "W-Left", "SnapToEdge", "direction", "left" },
	{ "W-Right", "SnapToEdge", "direction", "right" },
	{ "W-Up", "SnapToEdge", "direction", "up" },
	{ "W-Down", "SnapToEdge", "direction", "down" },
	{ "A-Space", "ShowMenu", "menu", "client-menu"},
	{ "XF86_AudioLowerVolume", "Execute", "command", "amixer sset Master 5%-" },
	{ "XF86_AudioRaiseVolume", "Execute", "command", "amixer sset Master 5%+" },
	{ "XF86_AudioMute", "Execute", "command", "amixer sset Master toggle" },
	{ "XF86_MonBrightnessUp", "Execute", "command", "brightnessctl set +10%" },
	{ "XF86_MonBrightnessDown", "Execute", "command", "brightnessctl set 10%-" },
	{ NULL, NULL, NULL, NULL },
};

static void
load_default_key_bindings(void)
{
	struct keybind *k;
	struct action *action;
	for (int i = 0; key_combos[i].binding; i++) {
		k = keybind_create(key_combos[i].binding);
		if (!k) {
			continue;
		}

		action = action_create(key_combos[i].action);
		wl_list_append(&k->actions, &action->link);

		if (key_combos[i].attribute && key_combos[i].value) {
			action_arg_from_xml_node(action,
				key_combos[i].attribute, key_combos[i].value);
		}
	}
}

/*
 * `struct mouse_combo` variable description and examples:
 *
 * | Variable  | Description                | Examples
 * |-----------|----------------------------|---------------------
 * | context   | context name               | Maximize, Root
 * | button    | mousebind button/direction | Left, Up
 * | event     | mousebind action           | Click, Scroll
 * | action    | action name                | ToggleMaximize, GoToDesktop
 * | attribute | action attribute           | to
 * | value     | action attribute value     | left
 *
 * <mouse>
 *   <context name="Maximize">
 *     <mousebind button="Left" action="Click">
 *       <action name="Focus"/>
 *       <action name="Raise"/>
 *       <action name="ToggleMaximize"/>
 *     </mousebind>
 *   </context>
 *   <context name="Root">
 *     <mousebind direction="Up" action="Scroll">
 *       <action name="GoToDesktop" to="left" wrap="yes"/>
 *     </mousebind>
 *   </context>
 * </mouse>
 */
static struct mouse_combos {
	const char *context, *button, *event, *action, *attribute, *value;
} mouse_combos[] = {
	{ "Left", "Left", "Drag", "Resize", NULL, NULL},
	{ "Top", "Left", "Drag", "Resize", NULL, NULL},
	{ "Bottom", "Left", "Drag", "Resize", NULL, NULL},
	{ "Right", "Left", "Drag", "Resize", NULL, NULL},
	{ "TLCorner", "Left", "Drag", "Resize", NULL, NULL},
	{ "TRCorner", "Left", "Drag", "Resize", NULL, NULL},
	{ "BRCorner", "Left", "Drag", "Resize", NULL, NULL},
	{ "BLCorner", "Left", "Drag", "Resize", NULL, NULL},
	{ "Frame", "A-Left", "Press", "Focus", NULL, NULL},
	{ "Frame", "A-Left", "Press", "Raise", NULL, NULL},
	{ "Frame", "A-Left", "Drag", "Move", NULL, NULL},
	{ "Frame", "A-Right", "Press", "Focus", NULL, NULL},
	{ "Frame", "A-Right", "Press", "Raise", NULL, NULL},
	{ "Frame", "A-Right", "Drag", "Resize", NULL, NULL},
	{ "Titlebar", "Left", "Press", "Focus", NULL, NULL},
	{ "Titlebar", "Left", "Press", "Raise", NULL, NULL},
	{ "Titlebar", "Up", "Scroll", "Unfocus", NULL, NULL},
	{ "Titlebar", "Up", "Scroll", "Shade", NULL, NULL},
	{ "Titlebar", "Down", "Scroll", "Unshade", NULL, NULL},
	{ "Titlebar", "Down", "Scroll", "Focus", NULL, NULL},
	{ "Title", "Left", "Drag", "Move", NULL, NULL },
	{ "Title", "Left", "DoubleClick", "ToggleMaximize", NULL, NULL },
	{ "TitleBar", "Right", "Click", "Focus", NULL, NULL},
	{ "TitleBar", "Right", "Click", "Raise", NULL, NULL},
	{ "Title", "Right", "Click", "ShowMenu", "menu", "client-menu"},
	{ "Close", "Left", "Click", "Close", NULL, NULL },
	{ "Iconify", "Left", "Click", "Iconify", NULL, NULL},
	{ "Maximize", "Left", "Click", "ToggleMaximize", NULL, NULL},
	{ "Maximize", "Right", "Click", "ToggleMaximize", "direction", "horizontal"},
	{ "Maximize", "Middle", "Click", "ToggleMaximize", "direction", "vertical"},
	{ "WindowMenu", "Left", "Click", "ShowMenu", "menu", "client-menu"},
	{ "WindowMenu", "Right", "Click", "ShowMenu", "menu", "client-menu"},
	{ "Root", "Left", "Press", "ShowMenu", "menu", "root-menu"},
	{ "Root", "Right", "Press", "ShowMenu", "menu", "root-menu"},
	{ "Root", "Middle", "Press", "ShowMenu", "menu", "root-menu"},
	{ "Root", "Up", "Scroll", "GoToDesktop", "to", "left"},
	{ "Root", "Down", "Scroll", "GoToDesktop", "to", "right"},
	{ "Client", "Left", "Press", "Focus", NULL, NULL},
	{ "Client", "Left", "Press", "Raise", NULL, NULL},
	{ "Client", "Right", "Press", "Focus", NULL, NULL},
	{ "Client", "Right", "Press", "Raise", NULL, NULL},
	{ "Client", "Middle", "Press", "Focus", NULL, NULL},
	{ "Client", "Middle", "Press", "Raise", NULL, NULL},
	{ NULL, NULL, NULL, NULL, NULL, NULL },
};

static void
load_default_mouse_bindings(void)
{
	uint32_t count = 0;
	struct mousebind *m;
	struct action *action;
	struct mouse_combos *current;
	for (int i = 0; mouse_combos[i].context; i++) {
		current = &mouse_combos[i];
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

		/*
		 * Only one attribute/value (of string type) is required for the
		 * built-in binds.  If more are required in the future, a
		 * slightly more sophisticated approach will be needed.
		 */
		if (current->attribute && current->value) {
			action_arg_from_xml_node(action,
				current->attribute, current->value);
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
				free(existing);
				replaced++;
				break;
			}
		}
	}
	wl_list_for_each_safe(current, tmp, &rc.keybinds, link) {
		if (wl_list_empty(&current->actions)) {
			wl_list_remove(&current->link);
			free(current);
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
	if (!rc.font_menuitem.name) {
		rc.font_menuitem.name = xstrdup("sans");
	}
	if (!rc.font_osd.name) {
		rc.font_osd.name = xstrdup("sans");
	}
	if (!libinput_category_get_default()) {
		/* So we still allow tap to click by default */
		struct libinput_category *l = libinput_category_create();
		/* Prevents unused variable warning when compiled without asserts */
		(void)l;
		assert(l && libinput_category_get_default() == l);
	}

	int nr_workspaces = wl_list_length(&rc.workspace_config.workspaces);
	if (nr_workspaces < rc.workspace_config.min_nr_workspaces) {
		struct workspace *workspace;
		for (int i = nr_workspaces; i < rc.workspace_config.min_nr_workspaces; i++) {
			workspace = znew(*workspace);
			workspace->name = strdup_printf("Workspace %d", i + 1);
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
}

static void
rule_destroy(struct window_rule *rule)
{
	wl_list_remove(&rule->link);
	zfree(rule->identifier);
	zfree(rule->title);
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
		if (!rule->identifier && !rule->title) {
			wlr_log(WLR_ERROR, "Deleting rule %p as it has no criteria", rule);
			rule_destroy(rule);
		}
	}

	validate_actions();
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
	struct buf b;

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

		buf_init(&b);
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
		zfree(b.buf);
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
	zfree(rc.font_menuitem.name);
	zfree(rc.font_osd.name);
	zfree(rc.theme_name);

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
		zfree(k->keysyms);
		zfree(k);
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

	zfree(rc.tablet.output_name);

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
		zfree(field);
	}

	struct window_rule *rule, *rule_tmp;
	wl_list_for_each_safe(rule, rule_tmp, &rc.window_rules, link) {
		rule_destroy(rule);
	}

	/* Reset state vars for starting fresh when Reload is triggered */
	current_usable_area_override = NULL;
	current_keybind = NULL;
	current_mousebind = NULL;
	current_touch = NULL;
	current_libinput_category = NULL;
	current_mouse_context = NULL;
	current_keybind_action = NULL;
	current_mousebind_action = NULL;
	current_child_action = NULL;
	current_view_query = NULL;
	current_region = NULL;
	current_field = NULL;
	current_window_rule = NULL;
	current_window_rule_action = NULL;
}
