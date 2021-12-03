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
#include <wlr/util/log.h>
#include "common/dir.h"
#include "common/nodename.h"
#include "common/string-helpers.h"
#include "common/zfree.h"
#include "config/keybind.h"
#include "config/libinput.h"
#include "config/mousebind.h"
#include "config/rcxml.h"

static bool in_keybind;
static bool in_mousebind;
static bool in_libinput_category;
static struct keybind *current_keybind;
static struct mousebind *current_mousebind;
static struct libinput_category *current_libinput_category;
static const char *current_mouse_context;

enum font_place {
	FONT_PLACE_UNKNOWN = 0,
	FONT_PLACE_ACTIVEWINDOW,
	FONT_PLACE_MENUITEM,
	/* TODO: Add all places based on Openbox's rc.xml */
};

static void load_default_key_bindings(void);

static void
fill_keybind(char *nodename, char *content)
{
	if (!content) {
		return;
	}
	string_truncate_at_pattern(nodename, ".keybind.keyboard");
	if (!strcmp(nodename, "key")) {
		current_keybind = keybind_create(content);
	}
	/*
	 * We expect <keybind key=""> to come first
	 * If a invalid keybind has been provided, keybind_create() complains
	 * so we just silently ignore it here.
	 */
	if (!current_keybind) {
		return;
	}
	if (!strcmp(nodename, "name.action")) {
		current_keybind->action = strdup(content);
	} else if (!strcmp(nodename, "command.action")) {
		current_keybind->command = strdup(content);
	} else if (!strcmp(nodename, "direction.action")) {
		current_keybind->command = strdup(content);
	} else if (!strcmp(nodename, "menu.action")) {
		current_keybind->command = strdup(content);
	}
}

static void
fill_mousebind(char *nodename, char *content)
{
	/*
	 * Example of what we are parsing:
	 * <mousebind button="Left" action="DoubleClick">
	 *   <action name="ToggleMaximize"/>
	 * </mousebind>
	 */

	if (!strcmp(nodename, "mousebind.context.mouse")) {
		wlr_log(WLR_INFO, "create mousebind for %s",
			current_mouse_context);
		current_mousebind = mousebind_create(current_mouse_context);
	}
	if (!content) {
		return;
	}
	string_truncate_at_pattern(nodename, ".mousebind.context.mouse");

	if (!strcmp(nodename, "button")) {
		current_mousebind->button = mousebind_button_from_str(content,
			&current_mousebind->modifiers);
	} else if (!strcmp(nodename, "action")) {
		 /* <mousebind button="" action="EVENT"> */
		current_mousebind->mouse_event =
			mousebind_event_from_str(content);
	} else if (!strcmp(nodename, "name.action")) {
		current_mousebind->action = strdup(content);
	} else if (!strcmp(nodename, "command.action")) {
		current_mousebind->command = strdup(content);
	} else if (!strcmp(nodename, "menu.action")) {
		current_mousebind->command = strdup(content);
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
			current_libinput_category->name = strdup(content);
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
	} else if (!strcasecmp(nodename, "accelProfile")) {
		current_libinput_category->accel_profile = get_accel_profile(content);
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
fill_font(char *nodename, char *content, enum font_place place)
{
	if (!content) {
		return;
	}
	string_truncate_at_pattern(nodename, ".font.theme");

	switch (place) {
	case FONT_PLACE_UNKNOWN:
		/*
		 * If <theme><font></font></theme> is used without a place=""
		 * attribute, we set all font variables
		 */
		if (!strcmp(nodename, "name")) {
			rc.font_name_activewindow = strdup(content);
			rc.font_name_menuitem = strdup(content);
		} else if (!strcmp(nodename, "size")) {
			rc.font_size_activewindow = atoi(content);
			rc.font_size_menuitem = atoi(content);
		}
		break;
	case FONT_PLACE_ACTIVEWINDOW:
		if (!strcmp(nodename, "name")) {
			rc.font_name_activewindow = strdup(content);
		} else if (!strcmp(nodename, "size")) {
			rc.font_size_activewindow = atoi(content);
		}
		break;
	case FONT_PLACE_MENUITEM:
		if (!strcmp(nodename, "name")) {
			rc.font_name_menuitem = strdup(content);
		} else if (!strcmp(nodename, "size")) {
			rc.font_size_menuitem = atoi(content);
		}
		break;

		/* TODO: implement for all font places */

	default:
		break;
	}
}

static enum font_place
enum_font_place(const char *place)
{
	if (!place) {
		return FONT_PLACE_UNKNOWN;
	}
	if (!strcasecmp(place, "ActiveWindow")) {
		return FONT_PLACE_ACTIVEWINDOW;
	} else if (!strcasecmp(place, "MenuItem")) {
		return FONT_PLACE_MENUITEM;
	}
	return FONT_PLACE_UNKNOWN;
}

static void
entry(xmlNode *node, char *nodename, char *content)
{
	/* current <theme><font place=""></font></theme> */
	static enum font_place font_place = FONT_PLACE_UNKNOWN;

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

	/* handle nodes without content, e.g. <keyboard><default /> */
	if (!strcmp(nodename, "default.keyboard")) {
		load_default_key_bindings();
		return;
	}

	/* handle the rest */
	if (!content) {
		return;
	}
	if (!strcmp(nodename, "place.font.theme")) {
		font_place = enum_font_place(content);
	}

	if (!strcmp(nodename, "decoration.core")) {
		if (!strcmp(content, "client")) {
			rc.xdg_shell_server_side_deco = false;
		} else {
			rc.xdg_shell_server_side_deco = true;
		}
	} else if (!strcmp(nodename, "gap.core")) {
		rc.gap = atoi(content);
	} else if (!strcmp(nodename, "adaptiveSync.core")) {
		rc.adaptive_sync = get_bool(content);
	} else if (!strcmp(nodename, "name.theme")) {
		rc.theme_name = strdup(content);
	} else if (!strcmp(nodename, "cornerradius.theme")) {
		rc.corner_radius = atoi(content);
	} else if (!strcmp(nodename, "name.font.theme")) {
		fill_font(nodename, content, font_place);
	} else if (!strcmp(nodename, "size.font.theme")) {
		fill_font(nodename, content, font_place);
	} else if (!strcasecmp(nodename, "followMouse.focus")) {
		rc.focus_follow_mouse = get_bool(content);
	} else if (!strcasecmp(nodename, "raiseOnFocus.focus")) {
		rc.raise_on_focus = get_bool(content);
		if (rc.raise_on_focus) {
			rc.focus_follow_mouse = true;
		}
	} else if (!strcasecmp(nodename, "doubleClickTime.mouse")) {
		long doubleclick_time_parsed = strtol(content, NULL, 10);
		if (doubleclick_time_parsed > 0) {
			rc.doubleclick_time = doubleclick_time_parsed;
		} else {
			wlr_log(WLR_ERROR, "invalid doubleClickTime");
		}
	} else if (!strcasecmp(nodename, "name.context.mouse")) {
		current_mouse_context = content;
	} else if (!strcasecmp(nodename, "repeatRate.keyboard")) {
		rc.repeat_rate = atoi(content);
	} else if (!strcasecmp(nodename, "repeatDelay.keyboard")) {
		rc.repeat_delay = atoi(content);
	} else if (!strcasecmp(nodename, "screenEdgeStrength.resistance")) {
		rc.screen_edge_strength = atoi(content);
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
rcxml_init()
{
	static bool has_run;

	if (has_run) {
		return;
	}
	has_run = true;
	wl_list_init(&rc.keybinds);
	wl_list_init(&rc.mousebinds);
	wl_list_init(&rc.libinput_categories);
	rc.xdg_shell_server_side_deco = true;
	rc.corner_radius = 8;
	rc.font_size_activewindow = 10;
	rc.font_size_menuitem = 10;
	rc.doubleclick_time = 500;
	rc.repeat_rate = 25;
	rc.repeat_delay = 600;
	rc.screen_edge_strength = 20;
}

static struct {
	const char *binding, *action, *command;
} key_combos[] = {
	{ "A-Tab", "NextWindow", NULL },
	{ "A-Escape", "Exit", NULL },
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
	for (int i = 0; key_combos[i].binding; i++) {
		struct keybind *k = keybind_create(key_combos[i].binding);
		if (!k) {
			continue;
		}
		k->action = strdup(key_combos[i].action);
		if (key_combos[i].command) {
			k->command = strdup(key_combos[i].command);
		}
	}
}

static struct {
	const char *context, *button, *event, *action, *command;
} mouse_combos[] = {
	{ "Frame", "A-Left", "Press", "Focus", NULL},
	{ "Frame", "A-Left", "Press", "Raise", NULL},
	{ "Frame", "A-Left", "Press", "Move", NULL},
	{ "Frame", "A-Right", "Press", "Focus", NULL},
	{ "Frame", "A-Right", "Press", "Raise", NULL},
	{ "Frame", "A-Right", "Press", "Resize", NULL},
	{ "TitleBar", "Left", "Press", "Move", NULL },
	{ "TitleBar", "Left", "DoubleClick", "ToggleMaximize", NULL },
	{ "Close", "Left", "Click", "Close", NULL },
	{ "Iconify", "Left", "Click", "Iconify", NULL},
	{ "Maximize", "Left", "Click", "ToggleMaximize", NULL},
	{ "Root", "Left", "Press", "ShowMenu", "root-menu"},
	{ "Root", "Right", "Press", "ShowMenu", "root-menu"},
	{ "Root", "Middle", "Press", "ShowMenu", "root-menu"},
	{ "Client", "Left", "Press", "Focus", NULL},
	{ "Client", "Left", "Press", "Raise", NULL},
	{ "Titlebar", "Left", "Press", "Focus", NULL},
	{ "Titlebar", "Left", "Press", "Raise", NULL},
	{ NULL, NULL, NULL, NULL, NULL },
};

static void
load_default_mouse_bindings(void)
{
	for (int i = 0; mouse_combos[i].context; i++) {
		struct mousebind *m = mousebind_create(mouse_combos[i].context);
		m->button = mousebind_button_from_str(mouse_combos[i].button,
			&m->modifiers);
		m->mouse_event = mousebind_event_from_str(mouse_combos[i].event);
		m->action = strdup(mouse_combos[i].action);
		if (mouse_combos[i].command) {
			m->command = strdup(mouse_combos[i].command);
		}
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

	if (!rc.font_name_activewindow) {
		rc.font_name_activewindow = strdup("sans");
	}
	if (!rc.font_name_menuitem) {
		rc.font_name_menuitem = strdup("sans");
	}
	if (!wl_list_length(&rc.libinput_categories)) {
		/* So we still allow tap to click by default */
		struct libinput_category *l = libinput_category_create();
		l->type = TOUCH_DEVICE;
	}
}

static void
rcxml_path(char *buf, size_t len)
{
	if (!strlen(config_dir())) {
		return;
	}
	snprintf(buf, len, "%s/rc.xml", config_dir());
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
		if (p)
			*p = '\0';
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
	zfree(rc.font_name_activewindow);
	zfree(rc.font_name_menuitem);
	zfree(rc.theme_name);

	struct keybind *k, *k_tmp;
	wl_list_for_each_safe(k, k_tmp, &rc.keybinds, link) {
		wl_list_remove(&k->link);
		zfree(k->command);
		zfree(k->action);
		zfree(k->keysyms);
		zfree(k);
	}

	struct mousebind *m, *m_tmp;
	wl_list_for_each_safe(m, m_tmp, &rc.mousebinds, link) {
		wl_list_remove(&m->link);
		zfree(m->command);
		zfree(m->action);
		zfree(m);
	}

	struct libinput_category *l, *l_tmp;
	wl_list_for_each_safe(l, l_tmp, &rc.libinput_categories, link) {
		wl_list_remove(&l->link);
		zfree(l->name);
		zfree(l);
	}
}
