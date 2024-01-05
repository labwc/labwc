// SPDX-License-Identifier: GPL-2.0-only
/*
 * Theme engine for labwc
 *
 * Copyright (C) Johan Malm 2020-2023
 */

#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include <assert.h>
#include <cairo.h>
#include <drm_fourcc.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include <strings.h>
#include "common/macros.h"
#include "common/dir.h"
#include "common/font.h"
#include "common/graphic-helpers.h"
#include "common/match.h"
#include "common/mem.h"
#include "common/string-helpers.h"
#include "config/rcxml.h"
#include "button/button-png.h"

#if HAVE_RSVG
#include "button/button-svg.h"
#endif

#include "button/button-xbm.h"
#include "theme.h"
#include "buffer.h"
#include "ssd.h"
#include "view.h"
#include "labwc.h"
#include "window-rules.h"
struct button {
	const char *name;
	const char *alt_name;
	const char *fallback_button;  /* built-in 6x6 button */
	struct {
		struct lab_data_buffer **buffer;
		float *rgba;
	} active, inactive;
};

enum corner {
	LAB_CORNER_UNKNOWN = 0,
	LAB_CORNER_TOP_LEFT,
	LAB_CORNER_TOP_RIGHT,
};

struct rounded_corner_ctx {
	struct wlr_box *box;
	double radius;
	double line_width;
	float *fill_color;
	float *border_color;
	enum corner corner;
};

static struct lab_data_buffer *rounded_rect(struct rounded_corner_ctx *ctx);

static void
zdrop(struct lab_data_buffer **buffer)
{
	if (*buffer) {
		wlr_buffer_drop(&(*buffer)->base);
		*buffer = NULL;
	}
}

static enum corner
corner_from_icon_name(const char *icon_name)
{
	assert(icon_name);

	/*
	 * TODO: Once we implement titleLayout we can make the
	 *       return values depend on parsed config values.
	 */
	if (!strcmp(icon_name, "menu")) {
		return LAB_CORNER_TOP_LEFT;
	} else if (!strcmp(icon_name, "close")) {
		return LAB_CORNER_TOP_RIGHT;
	}
	return LAB_CORNER_UNKNOWN;
}

static void
create_hover_fallback(struct theme *theme, const char *icon_name,
		struct lab_data_buffer **hover_buffer,
		struct lab_data_buffer *icon_buffer)
{
	assert(icon_name);
	assert(icon_buffer);
	assert(!*hover_buffer);

	struct surface_context icon =
		get_cairo_surface_from_lab_data_buffer(icon_buffer);
	int icon_width = cairo_image_surface_get_width(icon.surface);
	int icon_height = cairo_image_surface_get_height(icon.surface);

	int width = SSD_BUTTON_WIDTH;
	int height = theme->title_height;

	if (width && height) {
		/*
		 * Proportionately increase size of hover_buffer if the
		 * non-hover 'donor' buffer is larger than the allocated space.
		 * It will get scaled down again by wlroots when rendered and as
		 * required by the current output scale.
		 *
		 * This ensures that icons > width or > height keep their aspect
		 * ratio and are rendered the same as without the hover overlay.
		 */
		double scale = MAX((double)icon_width / width,
				(double)icon_height / height);
		if (scale > 1.0f) {
			width = (double)width * scale;
			height = (double)height * scale;
		}
	}

	*hover_buffer = buffer_create_cairo(width, height, 1.0, true);

	cairo_t *cairo = (*hover_buffer)->cairo;
	cairo_surface_t *surf = cairo_get_target(cairo);

	/* Background */
	cairo_set_source_surface(cairo, icon.surface,
		(width - icon_width) / 2, (height - icon_height) / 2);
	cairo_paint(cairo);

	/* Overlay (non-multiplied alpha) */
	float overlay_color[4] = { 0.5f, 0.5f, 0.5f, 0.3f};
	enum corner corner = corner_from_icon_name(icon_name);

	if (corner == LAB_CORNER_UNKNOWN) {
		set_cairo_color(cairo, overlay_color);
		cairo_rectangle(cairo, 0, 0, width, height);
		cairo_fill(cairo);
	} else {
		struct rounded_corner_ctx rounded_ctx = {
			.box = &(struct wlr_box) { .width = width, .height = height },
			.radius = rc.corner_radius,
			.line_width = theme->border_width,
			.fill_color = overlay_color,
			.border_color = overlay_color,
			.corner = corner
		};
		struct lab_data_buffer *overlay_buffer = rounded_rect(&rounded_ctx);
		cairo_set_source_surface(cairo,
			cairo_get_target(overlay_buffer->cairo), 0, 0);
		cairo_paint(cairo);
		wlr_buffer_drop(&overlay_buffer->base);
	}
	cairo_surface_flush(surf);

	if (icon.is_duplicate) {
		cairo_surface_destroy(icon.surface);
	}
}

/*
 * We use the following button filename schema: "BUTTON [TOGGLED] [STATE]"
 * with the words separted by underscore, and the following meaning:
 *   - BUTTON can be one of 'max', 'iconify', 'close', 'menu'
 *   - TOGGLED is either 'toggled' or nothing
 *   - STATE is 'hover' or nothing. In future, 'pressed' may be supported too.
 *
 * We believe that this is how the vast majority of extant openbox themes out
 * there are constructed and it is consistent with the openbox.org wiki. But
 * please be aware that it is actually different to vanilla Openbox which uses:
 * "BUTTON [STATE] [TOGGLED]" following an unfortunate commit in 2014 which
 * broke themes and led to some distros patching Openbox:
 * https://github.com/danakj/openbox/commit/35e92e4c2a45b28d5c2c9b44b64aeb4222098c94
 *
 * Arch Linux and Debian patch Openbox to keep the old syntax (the one we use).
 * https://gitlab.archlinux.org/archlinux/packaging/packages/openbox/-/blob/main/debian-887908.patch?ref_type=heads
 * This patch does the following:
 *   - reads "%s_toggled_pressed.xbm" and "%s_toggled_hover.xbm" instead of the
 *     'hover_toggled' equivalents.
 *   - parses 'toggled.unpressed', toggled.pressed' and 'toggled.hover' instead
 *     of the other way around ('*.toggled') when processing themerc.
 *
 * For compatibility with distros which do not apply similar patches, we support
 * the hover-before-toggle too, for example:
 *
 *	.name = "max_toggled_hover",
 *	.alt_name = "max_hover_toggled",
 *
 * ...in the button array definition below.
 */
static void
load_buttons(struct theme *theme)
{
	struct button buttons[] = { {
		.name = "menu",
		.fallback_button = (const char[]){ 0x00, 0x18, 0x3c, 0x3c, 0x18, 0x00 },
		.active.buffer = &theme->button_menu_active_unpressed,
		.active.rgba = theme->window_active_button_menu_unpressed_image_color,
		.inactive.buffer = &theme->button_menu_inactive_unpressed,
		.inactive.rgba = theme->window_inactive_button_menu_unpressed_image_color,
	}, {
		.name = "iconify",
		.fallback_button = (const char[]){ 0x00, 0x00, 0x00, 0x00, 0x3f, 0x3f },
		.active.buffer = &theme->button_iconify_active_unpressed,
		.active.rgba = theme->window_active_button_iconify_unpressed_image_color,
		.inactive.buffer = &theme->button_iconify_inactive_unpressed,
		.inactive.rgba = theme->window_inactive_button_iconify_unpressed_image_color,
	}, {
		.name = "max",
		.fallback_button = (const char[]){ 0x3f, 0x3f, 0x21, 0x21, 0x21, 0x3f },
		.active.buffer = &theme->button_maximize_active_unpressed,
		.active.rgba = theme->window_active_button_max_unpressed_image_color,
		.inactive.buffer = &theme->button_maximize_inactive_unpressed,
		.inactive.rgba = theme->window_inactive_button_max_unpressed_image_color,
	}, {
		.name = "max_toggled",
		.fallback_button = (const char[]){ 0x3e, 0x22, 0x2f, 0x29, 0x39, 0x0f },
		.active.buffer = &theme->button_restore_active_unpressed,
		.active.rgba = theme->window_active_button_max_unpressed_image_color,
		.inactive.buffer = &theme->button_restore_inactive_unpressed,
		.inactive.rgba = theme->window_inactive_button_max_unpressed_image_color,
	}, {
		.name = "close",
		.fallback_button = (const char[]){ 0x33, 0x3f, 0x1e, 0x1e, 0x3f, 0x33 },
		.active.buffer = &theme->button_close_active_unpressed,
		.active.rgba = theme->window_active_button_close_unpressed_image_color,
		.inactive.buffer = &theme->button_close_inactive_unpressed,
		.inactive.rgba = theme->window_inactive_button_close_unpressed_image_color,
	}, {
		.name = "menu_hover",
		/* no fallback (non-hover variant is used instead) */
		.active.buffer = &theme->button_menu_active_hover,
		.active.rgba = theme->window_active_button_menu_unpressed_image_color,
		.inactive.buffer = &theme->button_menu_inactive_hover,
		.inactive.rgba = theme->window_inactive_button_menu_unpressed_image_color,
	}, {
		.name = "iconify_hover",
		/* no fallback (non-hover variant is used instead) */
		.active.buffer = &theme->button_iconify_active_hover,
		.active.rgba = theme->window_active_button_iconify_unpressed_image_color,
		.inactive.buffer = &theme->button_iconify_inactive_hover,
		.inactive.rgba = theme->window_inactive_button_iconify_unpressed_image_color,
	}, {
		.name = "max_hover",
		/* no fallback (non-hover variant is used instead) */
		.active.buffer = &theme->button_maximize_active_hover,
		.active.rgba = theme->window_active_button_max_unpressed_image_color,
		.inactive.buffer = &theme->button_maximize_inactive_hover,
		.inactive.rgba = theme->window_inactive_button_max_unpressed_image_color,
	}, {
		.name = "max_toggled_hover",
		.alt_name = "max_hover_toggled",
		/* no fallback (non-hover variant is used instead) */
		.active.buffer = &theme->button_restore_active_hover,
		.active.rgba = theme->window_active_button_max_unpressed_image_color,
		.inactive.buffer = &theme->button_restore_inactive_hover,
		.inactive.rgba = theme->window_inactive_button_max_unpressed_image_color,
	}, {
		.name = "close_hover",
		/* no fallback (non-hover variant is used instead) */
		.active.buffer = &theme->button_close_active_hover,
		.active.rgba = theme->window_active_button_close_unpressed_image_color,
		.inactive.buffer = &theme->button_close_inactive_hover,
		.inactive.rgba = theme->window_inactive_button_close_unpressed_image_color,
	}, };

	char filename[4096] = {0};
	for (size_t i = 0; i < ARRAY_SIZE(buttons); ++i) {
		struct button *b = &buttons[i];

		zdrop(b->active.buffer);
		zdrop(b->inactive.buffer);

		/* PNG */
		snprintf(filename, sizeof(filename), "%s-active.png", b->name);
		button_png_load(filename, b->active.buffer);
		snprintf(filename, sizeof(filename), "%s-inactive.png", b->name);
		button_png_load(filename, b->inactive.buffer);

#if HAVE_RSVG
		/* SVG */
		int size = theme->title_height - 2 * theme->padding_height;
		if (!*b->active.buffer) {
			snprintf(filename, sizeof(filename), "%s-active.svg", b->name);
			button_svg_load(filename, b->active.buffer, size);
		}
		if (!*b->inactive.buffer) {
			snprintf(filename, sizeof(filename), "%s-inactive.svg", b->name);
			button_svg_load(filename, b->inactive.buffer, size);
		}
#endif

		/* XBM */
		snprintf(filename, sizeof(filename), "%s.xbm", b->name);
		if (!*b->active.buffer) {
			button_xbm_load(filename, b->active.buffer, b->active.rgba);
		}
		if (!*b->inactive.buffer) {
			button_xbm_load(filename, b->inactive.buffer, b->inactive.rgba);
		}

		/*
		 * XBM (alternative name)
		 * For example max_hover_toggled instead of max_toggled_hover
		 */
		if (b->alt_name) {
			snprintf(filename, sizeof(filename), "%s.xbm", b->alt_name);
		}  else {
			filename[0] = '\0';
		}
		if (!*b->active.buffer) {
			button_xbm_load(filename, b->active.buffer, b->active.rgba);
		}
		if (!*b->inactive.buffer) {
			button_xbm_load(filename, b->inactive.buffer, b->inactive.rgba);
		}

		/*
		 * Builtin bitmap
		 *
		 * Applicable to basic buttons such as max, max_toggled and
		 * iconify. There are no bitmap fallbacks for *_hover icons.
		 */
		if (!b->fallback_button) {
			continue;
		}
		if (!*b->active.buffer) {
			button_xbm_from_bitmap(b->fallback_button,
				b->active.buffer, b->active.rgba);
		}
		if (!*b->inactive.buffer) {
			button_xbm_from_bitmap(b->fallback_button,
				b->inactive.buffer, b->inactive.rgba);
		}
	}

	/*
	 * If hover-icons do not exist, add fallbacks by copying the non-hover
	 * variant (base) and then adding an overlay.
	 */
	for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
		struct button *hover_button = &buttons[i];

		if (!strstr(hover_button->name, "_hover")) {
			continue;
		}

		/* If name=='foo_hover', basename='foo' */
		char basename[64]  = {0};
		snprintf(basename, sizeof(basename), "%s", hover_button->name);
		trim_last_field(basename, '_');
		for (size_t j = 0; j < ARRAY_SIZE(buttons); j++) {
			struct button *base = &buttons[j];
			if (!strcmp(basename, base->name)) {
				if (!*hover_button->active.buffer) {
					create_hover_fallback(theme, basename,
						hover_button->active.buffer,
						*base->active.buffer);
				}
				if (!*hover_button->inactive.buffer) {
					create_hover_fallback(theme, basename,
						hover_button->inactive.buffer,
						*base->inactive.buffer);
				}
				break;
			}
		}
	}
}

static int
hex_to_dec(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return 0;
}

/**
 * parse_hexstr - parse #rrggbb
 * @hex: hex string to be parsed
 * @rgba: pointer to float[4] for return value
 */
void
theme_parse_hexstr(const char *hex, float *rgba)
{
	if (!hex || hex[0] != '#' || strlen(hex) < 7) {
		return;
	}
	rgba[0] = (hex_to_dec(hex[1]) * 16 + hex_to_dec(hex[2])) / 255.0;
	rgba[1] = (hex_to_dec(hex[3]) * 16 + hex_to_dec(hex[4])) / 255.0;
	rgba[2] = (hex_to_dec(hex[5]) * 16 + hex_to_dec(hex[6])) / 255.0;
	if (strlen(hex) > 7) {
		rgba[3] = atoi(hex + 7) / 100.0;
	} else {
		rgba[3] = 1.0;
	}
}

static enum lab_justification
parse_justification(const char *str)
{
	if (!strcasecmp(str, "Center")) {
		return LAB_JUSTIFY_CENTER;
	} else if (!strcasecmp(str, "Right")) {
		return LAB_JUSTIFY_RIGHT;
	} else {
		return LAB_JUSTIFY_LEFT;
	}
}

/*
 * We generally use Openbox defaults, but if no theme file can be found it's
 * better to populate the theme variables with some sane values as no-one
 * wants to use openbox without a theme - it'll all just be black and white.
 *
 * Openbox doesn't actual start if it can't find a theme. As it's normally
 * packaged with Clearlooks, this is not a problem, but for labwc I thought
 * this was a bit hard-line. People might want to try labwc without having
 * Openbox (and associated themes) installed.
 *
 * theme_builtin() applies a theme that is similar to vanilla GTK
 */
static void
theme_builtin(struct theme *theme)
{
	theme->border_width = 1;
	theme->padding_height = 3;
	theme->title_height = INT_MIN;
	theme->menu_overlap_x = 0;
	theme->menu_overlap_y = 0;

	theme_parse_hexstr("#e1dedb", theme->window_active_border_color);
	theme_parse_hexstr("#f6f5f4", theme->window_inactive_border_color);

	theme_parse_hexstr("#ff0000", theme->window_toggled_keybinds_color);

	theme_parse_hexstr("#e1dedb", theme->window_active_title_bg_color);
	theme_parse_hexstr("#f6f5f4", theme->window_inactive_title_bg_color);

	theme_parse_hexstr("#000000", theme->window_active_label_text_color);
	theme_parse_hexstr("#000000", theme->window_inactive_label_text_color);
	theme->window_label_text_justify = parse_justification("Center");

	theme_parse_hexstr("#000000",
		theme->window_active_button_menu_unpressed_image_color);
	theme_parse_hexstr("#000000",
		theme->window_active_button_iconify_unpressed_image_color);
	theme_parse_hexstr("#000000",
		theme->window_active_button_max_unpressed_image_color);
	theme_parse_hexstr("#000000",
		theme->window_active_button_close_unpressed_image_color);
	theme_parse_hexstr("#000000",
		theme->window_inactive_button_menu_unpressed_image_color);
	theme_parse_hexstr("#000000",
		theme->window_inactive_button_iconify_unpressed_image_color);
	theme_parse_hexstr("#000000",
		theme->window_inactive_button_max_unpressed_image_color);
	theme_parse_hexstr("#000000",
		theme->window_inactive_button_close_unpressed_image_color);

	theme_parse_hexstr("#fcfbfa", theme->menu_items_bg_color);
	theme_parse_hexstr("#000000", theme->menu_items_text_color);
	theme_parse_hexstr("#e1dedb", theme->menu_items_active_bg_color);
	theme_parse_hexstr("#000000", theme->menu_items_active_text_color);

	theme->menu_item_padding_x = 7;
	theme->menu_item_padding_y = 4;

	theme->menu_min_width = 20;
	theme->menu_max_width = 200;

	theme->menu_separator_line_thickness = 1;
	theme->menu_separator_padding_width = 6;
	theme->menu_separator_padding_height = 3;
	theme_parse_hexstr("#888888", theme->menu_separator_color);

	theme->osd_window_switcher_width = 600;
	theme->osd_window_switcher_padding = 4;
	theme->osd_window_switcher_item_padding_x = 10;
	theme->osd_window_switcher_item_padding_y = 1;
	theme->osd_window_switcher_item_active_border_width = 2;

	theme->osd_workspace_switcher_boxes_width = 20;
	theme->osd_workspace_switcher_boxes_height = 20;

	/* inherit settings in post_processing() if not set elsewhere */
	theme->osd_bg_color[0] = FLT_MIN;
	theme->osd_border_width = INT_MIN;
	theme->osd_border_color[0] = FLT_MIN;
	theme->osd_label_text_color[0] = FLT_MIN;
}

static void
entry(struct theme *theme, const char *key, const char *value)
{
	if (!key || !value) {
		return;
	}

	/*
	 * Note that in order for the pattern match to apply to more than just
	 * the first instance, "else if" cannot be used throughout this function
	 */
	if (match_glob(key, "border.width")) {
		theme->border_width = atoi(value);
	}
	if (match_glob(key, "padding.height")) {
		theme->padding_height = atoi(value);
	}
	if (match_glob(key, "titlebar.height")) {
		theme->title_height = atoi(value);
	}
	if (match_glob(key, "menu.items.padding.x")) {
		theme->menu_item_padding_x = atoi(value);
	}
	if (match_glob(key, "menu.items.padding.y")) {
		theme->menu_item_padding_y = atoi(value);
	}
	if (match_glob(key, "menu.overlap.x")) {
		theme->menu_overlap_x = atoi(value);
	}
	if (match_glob(key, "menu.overlap.y")) {
		theme->menu_overlap_y = atoi(value);
	}

	if (match_glob(key, "window.active.border.color")) {
		theme_parse_hexstr(value, theme->window_active_border_color);
	}
	if (match_glob(key, "window.inactive.border.color")) {
		theme_parse_hexstr(value, theme->window_inactive_border_color);
	}
	/* border.color is obsolete, but handled for backward compatibility */
	if (match_glob(key, "border.color")) {
		theme_parse_hexstr(value, theme->window_active_border_color);
		theme_parse_hexstr(value, theme->window_inactive_border_color);
	}

	if (match_glob(key, "window.active.indicator.toggled-keybind.color")) {
		theme_parse_hexstr(value, theme->window_toggled_keybinds_color);
	}

	if (match_glob(key, "window.active.title.bg.color")) {
		theme_parse_hexstr(value, theme->window_active_title_bg_color);
	}
	if (match_glob(key, "window.inactive.title.bg.color")) {
		theme_parse_hexstr(value, theme->window_inactive_title_bg_color);
	}

	if (match_glob(key, "window.active.label.text.color")) {
		theme_parse_hexstr(value, theme->window_active_label_text_color);
	}
	if (match_glob(key, "window.inactive.label.text.color")) {
		theme_parse_hexstr(value, theme->window_inactive_label_text_color);
	}
	if (match_glob(key, "window.label.text.justify")) {
		theme->window_label_text_justify = parse_justification(value);
	}

	/* universal button */
	if (match_glob(key, "window.active.button.unpressed.image.color")) {
		theme_parse_hexstr(value,
			theme->window_active_button_menu_unpressed_image_color);
		theme_parse_hexstr(value,
			theme->window_active_button_iconify_unpressed_image_color);
		theme_parse_hexstr(value,
			theme->window_active_button_max_unpressed_image_color);
		theme_parse_hexstr(value,
			theme->window_active_button_close_unpressed_image_color);
	}
	if (match_glob(key, "window.inactive.button.unpressed.image.color")) {
		theme_parse_hexstr(value,
			theme->window_inactive_button_menu_unpressed_image_color);
		theme_parse_hexstr(value,
			theme->window_inactive_button_iconify_unpressed_image_color);
		theme_parse_hexstr(value,
			theme->window_inactive_button_max_unpressed_image_color);
		theme_parse_hexstr(value,
			theme->window_inactive_button_close_unpressed_image_color);
	}

	/* individual buttons */
	if (match_glob(key, "window.active.button.menu.unpressed.image.color")) {
		theme_parse_hexstr(value,
			theme->window_active_button_menu_unpressed_image_color);
	}
	if (match_glob(key, "window.active.button.iconify.unpressed.image.color")) {
		theme_parse_hexstr(value,
			theme->window_active_button_iconify_unpressed_image_color);
	}
	if (match_glob(key, "window.active.button.max.unpressed.image.color")) {
		theme_parse_hexstr(value,
			theme->window_active_button_max_unpressed_image_color);
	}
	if (match_glob(key, "window.active.button.close.unpressed.image.color")) {
		theme_parse_hexstr(value,
			theme->window_active_button_close_unpressed_image_color);
	}
	if (match_glob(key, "window.inactive.button.menu.unpressed.image.color")) {
		theme_parse_hexstr(value,
			theme->window_inactive_button_menu_unpressed_image_color);
	}
	if (match_glob(key, "window.inactive.button.iconify.unpressed.image.color")) {
		theme_parse_hexstr(value,
			theme->window_inactive_button_iconify_unpressed_image_color);
	}
	if (match_glob(key, "window.inactive.button.max.unpressed.image.color")) {
		theme_parse_hexstr(value,
			theme->window_inactive_button_max_unpressed_image_color);
	}
	if (match_glob(key, "window.inactive.button.close.unpressed.image.color")) {
		theme_parse_hexstr(value,
			theme->window_inactive_button_close_unpressed_image_color);
	}

	if (match_glob(key, "menu.width.min")) {
		theme->menu_min_width = atoi(value);
	}
	if (match_glob(key, "menu.width.max")) {
		theme->menu_max_width = atoi(value);
	}

	if (match_glob(key, "menu.items.bg.color")) {
		theme_parse_hexstr(value, theme->menu_items_bg_color);
	}
	if (match_glob(key, "menu.items.text.color")) {
		theme_parse_hexstr(value, theme->menu_items_text_color);
	}
	if (match_glob(key, "menu.items.active.bg.color")) {
		theme_parse_hexstr(value, theme->menu_items_active_bg_color);
	}
	if (match_glob(key, "menu.items.active.text.color")) {
		theme_parse_hexstr(value, theme->menu_items_active_text_color);
	}

	if (match_glob(key, "menu.separator.width")) {
		theme->menu_separator_line_thickness = atoi(value);
	}
	if (match_glob(key, "menu.separator.padding.width")) {
		theme->menu_separator_padding_width = atoi(value);
	}
	if (match_glob(key, "menu.separator.padding.height")) {
		theme->menu_separator_padding_height = atoi(value);
	}
	if (match_glob(key, "menu.separator.color")) {
		theme_parse_hexstr(value, theme->menu_separator_color);
	}

	if (match_glob(key, "osd.bg.color")) {
		theme_parse_hexstr(value, theme->osd_bg_color);
	}
	if (match_glob(key, "osd.border.width")) {
		theme->osd_border_width = atoi(value);
	}
	if (match_glob(key, "osd.border.color")) {
		theme_parse_hexstr(value, theme->osd_border_color);
	}
	if (match_glob(key, "osd.window-switcher.width")) {
		theme->osd_window_switcher_width = atoi(value);
	}
	if (match_glob(key, "osd.window-switcher.padding")) {
		theme->osd_window_switcher_padding = atoi(value);
	}
	if (match_glob(key, "osd.window-switcher.item.padding.x")) {
		theme->osd_window_switcher_item_padding_x = atoi(value);
	}
	if (match_glob(key, "osd.window-switcher.item.padding.y")) {
		theme->osd_window_switcher_item_padding_y = atoi(value);
	}
	if (match_glob(key, "osd.window-switcher.item.active.border.width")) {
		theme->osd_window_switcher_item_active_border_width = atoi(value);
	}
	if (match_glob(key, "osd.workspace-switcher.boxes.width")) {
		theme->osd_workspace_switcher_boxes_width = atoi(value);
	}
	if (match_glob(key, "osd.workspace-switcher.boxes.height")) {
		theme->osd_workspace_switcher_boxes_height = atoi(value);
	}
	if (match_glob(key, "osd.label.text.color")) {
		theme_parse_hexstr(value, theme->osd_label_text_color);
	}
}

static void
parse_config_line(char *line, char **key, char **value)
{
	char *p = strchr(line, ':');
	if (!p) {
		return;
	}
	*p = '\0';
	*key = string_strip(line);
	*value = string_strip(++p);
}

static void
process_line(struct theme *theme, char *line)
{
	if (line[0] == '\0' || line[0] == '#') {
		return;
	}
	char *key = NULL, *value = NULL;
	parse_config_line(line, &key, &value);
	entry(theme, key, value);
}

static void
theme_read(struct theme *theme, struct wl_list *paths)
{
	bool should_merge_config = rc.merge_config;
	struct wl_list *(*iter)(struct wl_list *list);
	iter = should_merge_config ? paths_get_prev : paths_get_next;

	for (struct wl_list *elm = iter(paths); elm != paths; elm = iter(elm)) {
		struct path *path = wl_container_of(elm, path, link);
		FILE *stream = fopen(path->string, "r");
		if (!stream) {
			continue;
		}

		wlr_log(WLR_INFO, "read theme %s", path->string);

		char *line = NULL;
		size_t len = 0;
		while (getline(&line, &len, stream) != -1) {
			char *p = strrchr(line, '\n');
			if (p) {
				*p = '\0';
			}
			process_line(theme, line);
		}
		zfree(line);
		fclose(stream);
		if (!should_merge_config) {
			break;
		}
	}
}

static struct lab_data_buffer *
rounded_rect(struct rounded_corner_ctx *ctx)
{
	/* 1 degree in radians (=2π/360) */
	double deg = 0.017453292519943295;

	if (ctx->corner == LAB_CORNER_UNKNOWN) {
		return NULL;
	}

	double w = ctx->box->width;
	double h = ctx->box->height;
	double r = ctx->radius;

	struct lab_data_buffer *buffer;
	/* TODO: scale */
	buffer = buffer_create_cairo(w, h, 1, /*free_on_destroy*/ true);

	cairo_t *cairo = buffer->cairo;
	cairo_surface_t *surf = cairo_get_target(cairo);

	/* set transparent background */
	cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cairo);

	/*
	 * Create outline path and fill. Illustration of top-left corner buffer:
	 *
	 *          _,,ooO"""""""""+
	 *        ,oO"'   ^        |
	 *      ,o"       |        |
	 *     o"         |r       |
	 *    o'          |        |
	 *    O     r     v        |
	 *    O<--------->+        |
	 *    O                    |
	 *    O                    |
	 *    O                    |
	 *    +--------------------+
	 */
	cairo_set_line_width(cairo, 0.0);
	cairo_new_sub_path(cairo);
	switch (ctx->corner) {
	case LAB_CORNER_TOP_LEFT:
		cairo_arc(cairo, r, r, r, 180 * deg, 270 * deg);
		cairo_line_to(cairo, w, 0);
		cairo_line_to(cairo, w, h);
		cairo_line_to(cairo, 0, h);
		break;
	case LAB_CORNER_TOP_RIGHT:
		cairo_arc(cairo, w - r, r, r, -90 * deg, 0 * deg);
		cairo_line_to(cairo, w, h);
		cairo_line_to(cairo, 0, h);
		cairo_line_to(cairo, 0, 0);
		break;
	default:
		wlr_log(WLR_ERROR, "unknown corner type");
	}
	cairo_close_path(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	set_cairo_color(cairo, ctx->fill_color);
	cairo_fill_preserve(cairo);
	cairo_stroke(cairo);

	/*
	 * Stroke horizontal and vertical borders, shown by Xs and Ys
	 * respectively in the figure below:
	 *
	 *          _,,ooO"XXXXXXXXX
	 *        ,oO"'            |
	 *      ,o"                |
	 *     o"                  |
	 *    o'                   |
	 *    O                    |
	 *    Y                    |
	 *    Y                    |
	 *    Y                    |
	 *    Y                    |
	 *    Y--------------------+
	 */
	cairo_set_line_cap(cairo, CAIRO_LINE_CAP_BUTT);
	set_cairo_color(cairo, ctx->border_color);
	cairo_set_line_width(cairo, ctx->line_width);
	double half_line_width = ctx->line_width / 2.0;
	switch (ctx->corner) {
	case LAB_CORNER_TOP_LEFT:
		cairo_move_to(cairo, half_line_width, h);
		cairo_line_to(cairo, half_line_width, r);
		cairo_move_to(cairo, r, half_line_width);
		cairo_line_to(cairo, w, half_line_width);
		break;
	case LAB_CORNER_TOP_RIGHT:
		cairo_move_to(cairo, 0, half_line_width);
		cairo_line_to(cairo, w - r, half_line_width);
		cairo_move_to(cairo, w - half_line_width, r);
		cairo_line_to(cairo, w - half_line_width, h);
		break;
	default:
		wlr_log(WLR_ERROR, "unknown corner type");
	}
	cairo_stroke(cairo);

	/*
	 * If radius==0 the borders stroked above go right up to (and including)
	 * the corners, so there is not need to do any more.
	 */
	if (!r) {
		goto out;
	}

	/*
	 * Stroke the arc section of the border of the corner piece.
	 *
	 * Note: This figure is drawn at a more zoomed in scale compared with
	 * those above.
	 *
	 *                 ,,ooooO""  ^
	 *            ,ooo""'      |  |
	 *         ,oOO"           |  | line-thickness
	 *       ,OO"              |  |
	 *     ,OO"         _,,ooO""  v
	 *    ,O"         ,oO"'
	 *   ,O'        ,o"
	 *  ,O'        o"
	 *  o'        o'
	 *  O         O
	 *  O---------O            +
	 *       <----------------->
	 *          radius
	 *
	 * We handle the edge-case where line-thickness > radius by merely
	 * setting line-thickness = radius and in effect drawing a quadrant of a
	 * circle. In this case the X and Y borders butt up against the arc and
	 * overlap each other (as their line-thickessnes are greater than the
	 * linethickness of the arc). As a result, there is no inner rounded
	 * corners.
	 *
	 * So, in order to have inner rounded corners cornerRadius should be
	 * greater than border.width.
	 *
	 * Also, see diagrams in https://github.com/labwc/labwc/pull/990
	 */
	double line_width = MIN(ctx->line_width, r);
	cairo_set_line_width(cairo, line_width);
	half_line_width = line_width / 2.0;
	switch (ctx->corner) {
	case LAB_CORNER_TOP_LEFT:
		cairo_move_to(cairo, half_line_width, r);
		cairo_arc(cairo, r, r, r - half_line_width, 180 * deg, 270 * deg);
		break;
	case LAB_CORNER_TOP_RIGHT:
		cairo_move_to(cairo, w - r, half_line_width);
		cairo_arc(cairo, w - r, r, r - half_line_width, -90 * deg, 0 * deg);
		break;
	default:
		break;
	}
	cairo_stroke(cairo);

out:
	cairo_surface_flush(surf);

	return buffer;
}

static void
create_corners(struct theme *theme)
{
	struct wlr_box box = {
		.x = 0,
		.y = 0,
		.width = SSD_BUTTON_WIDTH + theme->border_width,
		.height = theme->title_height + theme->border_width,
	};

	struct rounded_corner_ctx ctx = {
		.box = &box,
		.radius = rc.corner_radius,
		.line_width = theme->border_width,
		.fill_color = theme->window_active_title_bg_color,
		.border_color = theme->window_active_border_color,
		.corner = LAB_CORNER_TOP_LEFT,
	};
	theme->corner_top_left_active_normal = rounded_rect(&ctx);

	ctx.fill_color = theme->window_inactive_title_bg_color,
	ctx.border_color = theme->window_inactive_border_color,
	theme->corner_top_left_inactive_normal = rounded_rect(&ctx);

	ctx.corner = LAB_CORNER_TOP_RIGHT;
	ctx.fill_color = theme->window_active_title_bg_color,
	ctx.border_color = theme->window_active_border_color,
	theme->corner_top_right_active_normal = rounded_rect(&ctx);

	ctx.fill_color = theme->window_inactive_title_bg_color,
	ctx.border_color = theme->window_inactive_border_color,
	theme->corner_top_right_inactive_normal = rounded_rect(&ctx);
}

static void
post_processing(struct theme *theme)
{
	int h = MAX(font_height(&rc.font_activewindow), font_height(&rc.font_inactivewindow));
	if (theme->title_height < h) {
		theme->title_height = h + 2 * theme->padding_height;
	}

	theme->osd_window_switcher_item_height = font_height(&rc.font_osd)
		+ 2 * theme->osd_window_switcher_item_padding_y
		+ 2 * theme->osd_window_switcher_item_active_border_width;

	if (rc.corner_radius >= theme->title_height) {
		rc.corner_radius = theme->title_height - 1;
	}

	if (theme->menu_max_width < theme->menu_min_width) {
		wlr_log(WLR_ERROR,
			"Adjusting menu.width.max: .max (%d) lower than .min (%d)",
			theme->menu_max_width, theme->menu_min_width);
		theme->menu_max_width = theme->menu_min_width;
	}

	/* Inherit OSD settings if not set */
	if (theme->osd_bg_color[0] == FLT_MIN) {
		memcpy(theme->osd_bg_color,
			theme->window_active_title_bg_color,
			sizeof(theme->osd_bg_color));
	}
	if (theme->osd_border_width == INT_MIN) {
		theme->osd_border_width = theme->border_width;
	}
	if (theme->osd_label_text_color[0] == FLT_MIN) {
		memcpy(theme->osd_label_text_color,
			theme->window_active_label_text_color,
			sizeof(theme->osd_label_text_color));
	}
	if (theme->osd_border_color[0] == FLT_MIN) {
		/*
		 * As per http://openbox.org/wiki/Help:Themes#osd.border.color
		 * we should fall back to window_active_border_color but
		 * that is usually the same as window_active_title_bg_color
		 * and thus the fallback for osd_bg_color. Which would mean
		 * they are both the same color and thus the border is invisible.
		 *
		 * Instead, we fall back to osd_label_text_color which in turn
		 * falls back to window_active_label_text_color.
		 */
		memcpy(theme->osd_border_color, theme->osd_label_text_color,
			sizeof(theme->osd_border_color));
	}
	if (theme->osd_workspace_switcher_boxes_width == 0) {
		theme->osd_workspace_switcher_boxes_height = 0;
	}
	if (theme->osd_workspace_switcher_boxes_height == 0) {
		theme->osd_workspace_switcher_boxes_width = 0;
	}
}

void
theme_init(struct theme *theme, const char *theme_name)
{
	/*
	 * Set some default values. This is particularly important on
	 * reconfigure as not all themes set all options
	 */
	theme_builtin(theme);

	/* Read <data-dir>/share/themes/$theme_name/openbox-3/themerc */
	struct wl_list paths;
	paths_theme_create(&paths, theme_name, "themerc");
	theme_read(theme, &paths);
	paths_destroy(&paths);

	/* Read <config-dir>/labwc/themerc-override */
	paths_config_create(&paths, "themerc-override");
	theme_read(theme, &paths);
	paths_destroy(&paths);

	post_processing(theme);
	create_corners(theme);
	load_buttons(theme);
}

void
theme_finish(struct theme *theme)
{
	zdrop(&theme->corner_top_left_active_normal);
	zdrop(&theme->corner_top_left_inactive_normal);
	zdrop(&theme->corner_top_right_active_normal);
	zdrop(&theme->corner_top_right_inactive_normal);
}

struct theme
get_theme_for_view(struct view *view)
{
	float *custom_color = window_rules_get_custom_border_color(view);
	if (!custom_color) {
		return *view->server->theme;
	}

	struct theme theme = { 0 };

	/* The same code as in theme_init*(), but with introducing custom color*/
	theme_builtin(&theme);

	struct wl_list paths;
	paths_theme_create(&paths, rc.theme_name, "themerc");
	theme_read(&theme, &paths);
	paths_destroy(&paths);

	paths_config_create(&paths, "themerc-override");
	theme_read(&theme, &paths);
	paths_destroy(&paths);

	memcpy(theme.window_active_border_color, custom_color, sizeof(float)*4);
	memcpy(theme.window_inactive_border_color, custom_color, sizeof(float)*4);
	memcpy(theme.window_active_title_bg_color, custom_color, sizeof(float)*4);
	memcpy(theme.window_inactive_title_bg_color, custom_color, sizeof(float)*4);
	memcpy(theme.osd_bg_color, custom_color, sizeof(float)*4);
	memcpy(theme.osd_border_color, custom_color, sizeof(float)*4);
	memcpy(theme.window_toggled_keybinds_color, custom_color, sizeof(float)*4);

	post_processing(&theme);
	create_corners(&theme);
	load_buttons(&theme);

	return theme;
}
