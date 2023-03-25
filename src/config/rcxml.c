// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include "action.h"
#include "common/list.h"
#include "common/mem.h"
#include "common/nodename.h"
#include "common/string-helpers.h"
#include "config/keybind.h"
#include "config/libinput.h"
#include "config/mousebind.h"
#include "config/rcxml.h"
#include "regions.h"
#include "workspaces.h"

static bool in_regions;
static bool in_keybind;
static bool in_mousebind;
static bool in_libinput_category;
static struct keybind *current_keybind;
static struct mousebind *current_mousebind;
static struct libinput_category *current_libinput_category;
static const char *current_mouse_context;
static struct action *current_keybind_action;
static struct action *current_mousebind_action;
static struct region *current_region;

enum font_place {
	FONT_PLACE_NONE = 0,
	FONT_PLACE_UNKNOWN,
	FONT_PLACE_ACTIVEWINDOW,
	FONT_PLACE_MENUITEM,
	FONT_PLACE_OSD,
	/* TODO: Add all places based on Openbox's rc.xml */
};

static void load_default_key_bindings(void);
static void load_default_mouse_bindings(void);

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

static bool
get_bool(const char *s)
{
	if (!s) {
		return false;
	}
	if (!strcasecmp(s, "yes")) {
		return true;
	}
	if (!strcasecmp(s, "true")) {
		return true;
	}
	return false;
}

static enum libinput_config_accel_profile
get_accel_profile(const char *s)
{
	if (!s) {
		return LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
	}
	if (!strcasecmp(s, "flat")) {
		return LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
	}
	if (!strcasecmp(s, "adaptive")) {
		return LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
	}
	return LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
}

static void
fill_libinput_category(char *nodename, char *content)
{
	if (!strcmp(nodename, "category.device.libinput")) {
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
		if (!strcmp(content, "touch")
				|| !strcmp(content, "non-touch")
				|| !strcmp(content, "default")) {
			current_libinput_category->type = get_device_type(content);
		} else {
			current_libinput_category->name = xstrdup(content);
		}
	} else if (!strcasecmp(nodename, "naturalScroll")) {
		current_libinput_category->natural_scroll =
			get_bool(content) ? 1 : 0;
	} else if (!strcasecmp(nodename, "leftHanded")) {
		current_libinput_category->left_handed = get_bool(content) ? 1 : 0;
	} else if (!strcasecmp(nodename, "pointerSpeed")) {
		current_libinput_category->pointer_speed = atof(content);
		if (current_libinput_category->pointer_speed < -1) {
			current_libinput_category->pointer_speed = -1;
		} else if (current_libinput_category->pointer_speed > 1) {
			current_libinput_category->pointer_speed = 1;
		}
	} else if (!strcasecmp(nodename, "tap")) {
		current_libinput_category->tap = get_bool(content) ?
			LIBINPUT_CONFIG_TAP_ENABLED :
			LIBINPUT_CONFIG_TAP_DISABLED;
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
	} else if (!strcasecmp(nodename, "accelProfile")) {
		current_libinput_category->accel_profile =
			get_accel_profile(content);
	} else if (!strcasecmp(nodename, "middleEmulation")) {
		current_libinput_category->middle_emu = get_bool(content) ?
			LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED :
			LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED;
	} else if (!strcasecmp(nodename, "disableWhileTyping")) {
		current_libinput_category->dwt = get_bool(content) ?
			LIBINPUT_CONFIG_DWT_ENABLED :
			LIBINPUT_CONFIG_DWT_DISABLED;
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
		set_font_attr(&rc.font_menuitem, nodename, content);
		set_font_attr(&rc.font_osd, nodename, content);
		break;
	case FONT_PLACE_ACTIVEWINDOW:
		set_font_attr(&rc.font_activewindow, nodename, content);
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
	} else if (!strcasecmp(place, "MenuItem")) {
		return FONT_PLACE_MENUITEM;
	} else if (!strcasecmp(place, "OnScreenDisplay")
			|| !strcasecmp(place, "OSD")) {
		return FONT_PLACE_OSD;
	}
	return FONT_PLACE_UNKNOWN;
}

static void
entry(xmlNode *node, char *nodename, char *content)
{
	/* current <theme><font place=""></font></theme> */
	static enum font_place font_place = FONT_PLACE_NONE;

	if (!nodename) {
		return;
	}
	string_truncate_at_pattern(nodename, ".openbox_config");
	string_truncate_at_pattern(nodename, ".labwc_config");

	if (getenv("LABWC_DEBUG_CONFIG_NODENAMES")) {
		printf("%s: %s\n", nodename, content);
	}

	if (in_keybind) {
		fill_keybind(nodename, content);
	}
	if (in_mousebind) {
		fill_mousebind(nodename, content);
	}
	if (in_libinput_category) {
		fill_libinput_category(nodename, content);
	}
	if (in_regions) {
		fill_region(nodename, content);
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
		rc.adaptive_sync = get_bool(content);
	} else if (!strcasecmp(nodename, "reuseOutputMode.core")) {
		rc.reuse_output_mode = get_bool(content);
	} else if (!strcmp(nodename, "name.theme")) {
		rc.theme_name = xstrdup(content);
	} else if (!strcmp(nodename, "cornerradius.theme")) {
		rc.corner_radius = atoi(content);
	} else if (!strcmp(nodename, "name.font.theme")) {
		fill_font(nodename, content, font_place);
	} else if (!strcmp(nodename, "size.font.theme")) {
		fill_font(nodename, content, font_place);
	} else if (!strcmp(nodename, "slant.font.theme")) {
		fill_font(nodename, content, font_place);
	} else if (!strcmp(nodename, "weight.font.theme")) {
		fill_font(nodename, content, font_place);
	} else if (!strcasecmp(nodename, "followMouse.focus")) {
		rc.focus_follow_mouse = get_bool(content);
	} else if (!strcasecmp(nodename, "raiseOnFocus.focus")) {
		rc.raise_on_focus = get_bool(content);
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
	} else if (!strcasecmp(nodename, "screenEdgeStrength.resistance")) {
		rc.screen_edge_strength = atoi(content);
	} else if (!strcasecmp(nodename, "range.snapping")) {
		rc.snap_edge_range = atoi(content);
	} else if (!strcasecmp(nodename, "topMaximize.snapping")) {
		rc.snap_top_maximize = get_bool(content);

	/* <windowSwitcher show="" preview="" outlines="" /> */
	} else if (!strcasecmp(nodename, "show.windowSwitcher.core")) {
		rc.cycle_view_osd = get_bool(content);
	} else if (!strcasecmp(nodename, "preview.windowSwitcher.core")) {
		rc.cycle_preview_contents = get_bool(content);
	} else if (!strcasecmp(nodename, "outlines.windowSwitcher.core")) {
		rc.cycle_preview_outlines = get_bool(content);

	/* The following three are for backward compatibility only */
	} else if (!strcasecmp(nodename, "cycleViewOSD.core")) {
		rc.cycle_view_osd = get_bool(content);
		wlr_log(WLR_ERROR, "<cycleViewOSD> is deprecated."
			" Use <windowSwitcher show=\"\" />");
	} else if (!strcasecmp(nodename, "cycleViewPreview.core")) {
		rc.cycle_preview_contents = get_bool(content);
		wlr_log(WLR_ERROR, "<cycleViewPreview> is deprecated."
			" Use <windowSwitcher preview=\"\" />");
	} else if (!strcasecmp(nodename, "cycleViewOutlines.core")) {
		rc.cycle_preview_outlines = get_bool(content);
		wlr_log(WLR_ERROR, "<cycleViewOutlines> is deprecated."
			" Use <windowSwitcher outlines=\"\" />");

	} else if (!strcasecmp(nodename, "name.names.desktops")) {
		struct workspace *workspace = znew(*workspace);
		workspace->name = xstrdup(content);
		wl_list_append(&rc.workspace_config.workspaces, &workspace->link);
	} else if (!strcasecmp(nodename, "popupTime.desktops")) {
		rc.workspace_config.popuptime = atoi(content);
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
		wl_list_init(&rc.keybinds);
		wl_list_init(&rc.mousebinds);
		wl_list_init(&rc.libinput_categories);
		wl_list_init(&rc.workspace_config.workspaces);
		wl_list_init(&rc.regions);
	}
	has_run = true;

	rc.xdg_shell_server_side_deco = true;
	rc.corner_radius = 8;

	init_font_defaults(&rc.font_activewindow);
	init_font_defaults(&rc.font_menuitem);
	init_font_defaults(&rc.font_osd);

	rc.doubleclick_time = 500;
	rc.scroll_factor = 1.0;
	rc.repeat_rate = 25;
	rc.repeat_delay = 600;
	rc.screen_edge_strength = 20;
	rc.snap_edge_range = 1;
	rc.snap_top_maximize = true;
	rc.cycle_view_osd = true;
	rc.cycle_preview_contents = true;
	rc.cycle_preview_outlines = true;
	rc.workspace_config.popuptime = INT_MIN;
}

static struct {
	const char *binding, *action, *command;
} key_combos[] = {
	{ "A-Tab", "NextWindow", NULL },
	{ "W-Return", "Execute", "alacritty" },
	{ "A-F3", "Execute", "bemenu-run" },
	{ "A-F4", "Close", NULL },
	{ "W-a", "ToggleMaximize", NULL },
	{ "A-Left", "MoveToEdge", "left" },
	{ "A-Right", "MoveToEdge", "right" },
	{ "A-Up", "MoveToEdge", "up" },
	{ "A-Down", "MoveToEdge", "down" },
	{ "W-Left", "SnapToEdge", "left" },
	{ "W-Right", "SnapToEdge", "right" },
	{ "W-Up", "SnapToEdge", "up" },
	{ "W-Down", "SnapToEdge", "down" },
	{ "A-Space", "ShowMenu", "client-menu"},
	{ "XF86_AudioLowerVolume", "Execute", "amixer sset Master 5%-" },
	{ "XF86_AudioRaiseVolume", "Execute", "amixer sset Master 5%+" },
	{ "XF86_AudioMute", "Execute", "amixer sset Master toggle" },
	{ "XF86_MonBrightnessUp", "Execute", "brightnessctl set +10%" },
	{ "XF86_MonBrightnessDown", "Execute", "brightnessctl set 10%-" },
	{ NULL, NULL, NULL },
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

		if (key_combos[i].command) {
			action_arg_add_str(action, NULL, key_combos[i].command);
		}
	}
}

static struct mouse_combos {
	const char *context, *button, *event, *action, *command;
} mouse_combos[] = {
	{ "Left", "Left", "Drag", "Resize", NULL},
	{ "Top", "Left", "Drag", "Resize", NULL},
	{ "Bottom", "Left", "Drag", "Resize", NULL},
	{ "Right", "Left", "Drag", "Resize", NULL},
	{ "TLCorner", "Left", "Drag", "Resize", NULL},
	{ "TRCorner", "Left", "Drag", "Resize", NULL},
	{ "BRCorner", "Left", "Drag", "Resize", NULL},
	{ "BLCorner", "Left", "Drag", "Resize", NULL},
	{ "Frame", "A-Left", "Press", "Focus", NULL},
	{ "Frame", "A-Left", "Press", "Raise", NULL},
	{ "Frame", "A-Left", "Drag", "Move", NULL},
	{ "Frame", "A-Right", "Press", "Focus", NULL},
	{ "Frame", "A-Right", "Press", "Raise", NULL},
	{ "Frame", "A-Right", "Drag", "Resize", NULL},
	{ "Titlebar", "Left", "Press", "Focus", NULL},
	{ "Titlebar", "Left", "Press", "Raise", NULL},
	{ "Title", "Left", "Drag", "Move", NULL },
	{ "Title", "Left", "DoubleClick", "ToggleMaximize", NULL },
	{ "TitleBar", "Right", "Click", "Focus", NULL},
	{ "TitleBar", "Right", "Click", "Raise", NULL},
	{ "TitleBar", "Right", "Click", "ShowMenu", "client-menu"},
	{ "Close", "Left", "Click", "Close", NULL },
	{ "Iconify", "Left", "Click", "Iconify", NULL},
	{ "Maximize", "Left", "Click", "ToggleMaximize", NULL},
	{ "WindowMenu", "Left", "Click", "ShowMenu", "client-menu"},
	{ "Root", "Left", "Press", "ShowMenu", "root-menu"},
	{ "Root", "Right", "Press", "ShowMenu", "root-menu"},
	{ "Root", "Middle", "Press", "ShowMenu", "root-menu"},
	{ "Root", "Up", "Scroll", "GoToDesktop", "left"},
	{ "Root", "Down", "Scroll", "GoToDesktop", "right"},
	{ "Client", "Left", "Press", "Focus", NULL},
	{ "Client", "Left", "Press", "Raise", NULL},
	{ "Client", "Right", "Press", "Focus", NULL},
	{ "Client", "Right", "Press", "Raise", NULL},
	{ "Client", "Middle", "Press", "Focus", NULL},
	{ "Client", "Middle", "Press", "Raise", NULL},
	{ NULL, NULL, NULL, NULL, NULL },
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

		if (current->command) {
			action_arg_add_str(action, NULL, current->command);
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
	if (!rc.font_menuitem.name) {
		rc.font_menuitem.name = xstrdup("sans");
	}
	if (!rc.font_osd.name) {
		rc.font_osd.name = xstrdup("sans");
	}
	if (!libinput_category_get_default()) {
		/* So we still allow tap to click by default */
		struct libinput_category *l = libinput_category_create();
		assert(l && libinput_category_get_default() == l);
	}
	if (!wl_list_length(&rc.workspace_config.workspaces)) {
		struct workspace *workspace = znew(*workspace);
		workspace->name = xstrdup("Default");
		wl_list_append(&rc.workspace_config.workspaces, &workspace->link);
	}
	if (rc.workspace_config.popuptime == INT_MIN) {
		rc.workspace_config.popuptime = 1000;
	}
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
}

static void
rcxml_path(char *buf, size_t len)
{
	if (!rc.config_dir) {
		return;
	}
	snprintf(buf, len, "%s/rc.xml", rc.config_dir);
}

static void
find_config_file(char *buffer, size_t len, const char *filename)
{
	if (filename) {
		snprintf(buffer, len, "%s", filename);
		return;
	}
	rcxml_path(buffer, len);
}

void
rcxml_read(const char *filename)
{
	FILE *stream;
	char *line = NULL;
	size_t len = 0;
	struct buf b;
	static char rcxml[4096] = {0};

	rcxml_init();

	/*
	 * rcxml_read() can be called multiple times, but we only set rcxml[]
	 * the first time. The specified 'filename' is only respected the first
	 * time.
	 */
	if (rcxml[0] == '\0') {
		find_config_file(rcxml, sizeof(rcxml), filename);
	}
	if (rcxml[0] == '\0') {
		wlr_log(WLR_INFO, "cannot find rc.xml config file");
		goto no_config;
	}

	/* Reading file into buffer before parsing - better for unit tests */
	stream = fopen(rcxml, "r");
	if (!stream) {
		wlr_log(WLR_ERROR, "cannot read (%s)", rcxml);
		goto no_config;
	}
	wlr_log(WLR_INFO, "read config file %s", rcxml);
	buf_init(&b);
	while (getline(&line, &len, stream) != -1) {
		char *p = strrchr(line, '\n');
		if (p) {
			*p = '\0';
		}
		buf_add(&b, line);
	}
	free(line);
	fclose(stream);
	rcxml_parse_xml(&b);
	free(b.buf);
no_config:
	post_processing();
}

void
rcxml_finish(void)
{
	zfree(rc.font_activewindow.name);
	zfree(rc.font_menuitem.name);
	zfree(rc.font_osd.name);
	zfree(rc.theme_name);

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

	/* Reset state vars for starting fresh when Reload is triggered */
	current_keybind = NULL;
	current_mousebind = NULL;
	current_libinput_category = NULL;
	current_mouse_context = NULL;
	current_keybind_action = NULL;
	current_mousebind_action = NULL;
	current_region = NULL;
}
