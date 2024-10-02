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
#include <wlr/render/pixman.h>
#include <strings.h>
#include "common/macros.h"
#include "common/dir.h"
#include "common/font.h"
#include "common/graphic-helpers.h"
#include "common/match.h"
#include "common/mem.h"
#include "common/parse-bool.h"
#include "common/parse-double.h"
#include "common/string-helpers.h"
#include "config/rcxml.h"
#include "img/img-png.h"
#include "labwc.h"

#if HAVE_RSVG
#include "img/img-svg.h"
#endif

#include "img/img-xbm.h"
#include "theme.h"
#include "buffer.h"
#include "ssd.h"

struct button {
	const char *name;
	const char *alt_name;
	const char *fallback_button;  /* built-in 6x6 button */
	struct {
		struct lab_data_buffer **buffer;
		float *rgba;
	} active, inactive;
};

struct rounded_corner_ctx {
	struct wlr_box *box;
	double radius;
	double line_width;
	float *fill_color;
	float *border_color;
	enum ssd_corner corner;
};

#define zero_array(arr) memset(arr, 0, sizeof(arr))

static struct lab_data_buffer *rounded_rect(struct rounded_corner_ctx *ctx);

/* 1 degree in radians (=2π/360) */
static const double deg = 0.017453292519943295;

static void
zdrop(struct lab_data_buffer **buffer)
{
	if (*buffer) {
		wlr_buffer_drop(&(*buffer)->base);
		*buffer = NULL;
	}
}

static struct lab_data_buffer *
create_hover_overlay(struct theme *theme, enum ssd_corner corner)
{
	int width = theme->window_button_width;
	int height = theme->title_height;

	struct lab_data_buffer *hover_buffer = buffer_create_cairo(width, height, 1.0, true);

	cairo_t *cairo = hover_buffer->cairo;
	cairo_surface_t *surf = cairo_get_target(cairo);

	float overlay_color[4] = { 0.15f, 0.15f, 0.15f, 0.3f};
	int radius = MIN(width, height) / 2;

	switch (theme->window_button_hover_bg_shape) {
	case LAB_CIRCLE:
		set_cairo_color(cairo, overlay_color);
		cairo_arc(cairo, width / 2, height / 2, radius, 0 * deg, 360 * deg);
		cairo_fill(cairo);
		break;
	case LAB_RECTANGLE:
		set_cairo_color(cairo, overlay_color);
		cairo_rectangle(cairo, 0, 0, width, height);
		if (corner == LAB_CORNER_UNKNOWN) {
			cairo_fill(cairo);
		} else {
			/*
			 * Round the hover overlay of corner buttons by
			 * cropping the region within the window border.
			 */
			float white[4] = {1, 1, 1, 1};
			struct rounded_corner_ctx rounded_ctx = {
				.box = &(struct wlr_box){
					.width = theme->padding_width + width,
					.height = height,
				},
				.radius = rc.corner_radius,
				.line_width = theme->border_width,
				.fill_color = white,
				.border_color = white,
				.corner = corner,
			};
			struct lab_data_buffer *mask_buffer =
				rounded_rect(&rounded_ctx);
			cairo_pattern_t *mask_pattern =
				cairo_pattern_create_for_surface(
					cairo_get_target(mask_buffer->cairo));
			int mask_offset;
			if (corner == LAB_CORNER_TOP_LEFT) {
				mask_offset = -theme->padding_width;
			} else {
				mask_offset = 0;
			}
			cairo_save(cairo);
			cairo_translate(cairo, mask_offset, 0);
			cairo_mask(cairo, mask_pattern);
			cairo_restore(cairo);
			wlr_buffer_drop(&mask_buffer->base);
		}
		break;
	}
	cairo_surface_flush(surf);

	return hover_buffer;
}

/*
 * Scan theme directories with button names (name + postfix) and write the full
 * path of the found button file to @buf. An empty string is set if a button
 * file is not found.
 */
static void
get_button_filename(char *buf, size_t len, const char *name, const char *postfix)
{
	buf[0] = '\0';

	char filename[4096];
	snprintf(filename, sizeof(filename), "%s%s", name, postfix);

	struct wl_list paths;
	paths_theme_create(&paths, rc.theme_name, filename);

	/*
	 * You can't really merge buttons, so let's just iterate forwards
	 * and stop on the first hit
	 */
	struct path *path;
	wl_list_for_each(path, &paths, link) {
		if (access(path->string, R_OK) == 0) {
			snprintf(buf, len, "%s", path->string);
			break;
		}
	}
	paths_destroy(&paths);
}

/*
 * We use the following button filename schema: "BUTTON [TOGGLED] [STATE]"
 * with the words separated by underscore, and the following meaning:
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
		.name = "shade",
		.fallback_button = (const char[]){ 0x3f, 0x3f, 0x00, 0x0c, 0x1e, 0x3f },
		.active.buffer = &theme->button_shade_active_unpressed,
		.active.rgba = theme->window_active_button_shade_unpressed_image_color,
		.inactive.buffer = &theme->button_shade_inactive_unpressed,
		.inactive.rgba = theme->window_inactive_button_shade_unpressed_image_color,
	}, {
		.name = "shade_toggled",
		.fallback_button = (const char[]){ 0x3f, 0x3f, 0x00, 0x3f, 0x1e, 0x0c },
		.active.buffer = &theme->button_unshade_active_unpressed,
		.active.rgba = theme->window_active_button_shade_unpressed_image_color,
		.inactive.buffer = &theme->button_unshade_inactive_unpressed,
		.inactive.rgba = theme->window_inactive_button_shade_unpressed_image_color,
	}, {
		.name = "desk",
		.fallback_button = (const char[]){ 0x33, 0x33, 0x00, 0x00, 0x33, 0x33 },
		.active.buffer = &theme->button_omnipresent_active_unpressed,
		.active.rgba = theme->window_active_button_omnipresent_unpressed_image_color,
		.inactive.buffer = &theme->button_omnipresent_inactive_unpressed,
		.inactive.rgba = theme->window_inactive_button_omnipresent_unpressed_image_color,
	}, {
		.name = "desk_toggled",
		.fallback_button = (const char[]){ 0x00, 0x1e, 0x1a, 0x16, 0x1e, 0x00 },
		.active.buffer = &theme->button_exclusive_active_unpressed,
		.active.rgba = theme->window_active_button_omnipresent_unpressed_image_color,
		.inactive.buffer = &theme->button_exclusive_inactive_unpressed,
		.inactive.rgba = theme->window_inactive_button_omnipresent_unpressed_image_color,
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
		.name = "shade_hover",
		/* no fallback (non-hover variant is used instead) */
		.active.buffer = &theme->button_shade_active_hover,
		.active.rgba = theme->window_active_button_shade_unpressed_image_color,
		.inactive.buffer = &theme->button_shade_inactive_hover,
		.inactive.rgba = theme->window_inactive_button_shade_unpressed_image_color,
	}, {
		.name = "shade_toggled_hover",
		.alt_name = "shade_hover_toggled",
		/* no fallback (non-hover variant is used instead) */
		.active.buffer = &theme->button_unshade_active_hover,
		.active.rgba = theme->window_active_button_shade_unpressed_image_color,
		.inactive.buffer = &theme->button_unshade_inactive_hover,
		.inactive.rgba = theme->window_inactive_button_shade_unpressed_image_color,
	}, {
		.name = "desk_hover",
		/* no fallback (non-hover variant is used instead) */
		.active.buffer = &theme->button_omnipresent_active_hover,
		.active.rgba = theme->window_active_button_omnipresent_unpressed_image_color,
		.inactive.buffer = &theme->button_omnipresent_inactive_hover,
		.inactive.rgba = theme->window_inactive_button_omnipresent_unpressed_image_color,
	}, {
		.name = "desk_toggled_hover",
		.alt_name = "desk_hover_toggled",
		/* no fallback (non-hover variant is used instead) */
		.active.buffer = &theme->button_exclusive_active_hover,
		.active.rgba = theme->window_active_button_omnipresent_unpressed_image_color,
		.inactive.buffer = &theme->button_exclusive_inactive_hover,
		.inactive.rgba = theme->window_inactive_button_omnipresent_unpressed_image_color,
	}, {
		.name = "close_hover",
		/* no fallback (non-hover variant is used instead) */
		.active.buffer = &theme->button_close_active_hover,
		.active.rgba = theme->window_active_button_close_unpressed_image_color,
		.inactive.buffer = &theme->button_close_inactive_hover,
		.inactive.rgba = theme->window_inactive_button_close_unpressed_image_color,
	}, };

	theme->button_hover_overlay_left =
		create_hover_overlay(theme, LAB_CORNER_TOP_LEFT);
	theme->button_hover_overlay_right =
		create_hover_overlay(theme, LAB_CORNER_TOP_RIGHT);
	theme->button_hover_overlay_middle =
		create_hover_overlay(theme, LAB_CORNER_UNKNOWN);

	char filename[4096] = {0};
	for (size_t i = 0; i < ARRAY_SIZE(buttons); ++i) {
		struct button *b = &buttons[i];

		zdrop(b->active.buffer);
		zdrop(b->inactive.buffer);

		/* PNG */
		get_button_filename(filename, sizeof(filename), b->name, "-active.png");
		img_png_load(filename, b->active.buffer);
		get_button_filename(filename, sizeof(filename), b->name, "-inactive.png");
		img_png_load(filename, b->inactive.buffer);

#if HAVE_RSVG
		/* SVG */
		int size = theme->title_height - 2 * theme->padding_height;
		if (!*b->active.buffer) {
			get_button_filename(filename, sizeof(filename), b->name, "-active.svg");
			img_svg_load(filename, b->active.buffer, size);
		}
		if (!*b->inactive.buffer) {
			get_button_filename(filename, sizeof(filename), b->name, "-inactive.svg");
			img_svg_load(filename, b->inactive.buffer, size);
		}
#endif

		/* XBM */
		get_button_filename(filename, sizeof(filename), b->name, ".xbm");
		if (!*b->active.buffer) {
			img_xbm_load(filename, b->active.buffer, b->active.rgba);
		}
		if (!*b->inactive.buffer) {
			img_xbm_load(filename, b->inactive.buffer, b->inactive.rgba);
		}

		/*
		 * XBM (alternative name)
		 * For example max_hover_toggled instead of max_toggled_hover
		 */
		if (b->alt_name) {
			get_button_filename(filename, sizeof(filename), b->alt_name, ".xbm");
		}  else {
			filename[0] = '\0';
		}
		if (!*b->active.buffer) {
			img_xbm_load(filename, b->active.buffer, b->active.rgba);
		}
		if (!*b->inactive.buffer) {
			img_xbm_load(filename, b->inactive.buffer, b->inactive.rgba);
		}

		/*
		 * Builtin bitmap
		 *
		 * Applicable to basic buttons such as max, max_toggled and
		 * iconify. There are no bitmap fallbacks for *_hover icons.
		 */
		if (!*b->active.buffer && b->fallback_button) {
			img_xbm_from_bitmap(b->fallback_button,
				b->active.buffer, b->active.rgba);
		}
		if (!*b->inactive.buffer && b->fallback_button) {
			img_xbm_from_bitmap(b->fallback_button,
				b->inactive.buffer, b->inactive.rgba);
		}

		/*
		 * When *_hover icon is not provided, set hover overlay instead.
		 * This is rendered on top of non-hover icons. Their shapes are
		 * updated on SSD creation.
		 */
		if (!*b->active.buffer) {
			*b->active.buffer = theme->button_hover_overlay_middle;
		}
		if (!*b->inactive.buffer) {
			*b->inactive.buffer = theme->button_hover_overlay_middle;
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
static void
parse_hexstr(const char *hex, float *rgba)
{
	if (!hex || hex[0] != '#' || strlen(hex) < 7) {
		return;
	}
	rgba[0] = (hex_to_dec(hex[1]) * 16 + hex_to_dec(hex[2])) / 255.0;
	rgba[1] = (hex_to_dec(hex[3]) * 16 + hex_to_dec(hex[4])) / 255.0;
	rgba[2] = (hex_to_dec(hex[5]) * 16 + hex_to_dec(hex[6])) / 255.0;
	rgba[3] = 1.0;

	size_t len = strlen(hex);
	if (len >= 9 && hex[7] == ' ') {
		/* Deprecated #aabbcc 100 alpha encoding to support openbox themes */
		rgba[3] = atoi(hex + 8) / 100.0;
		wlr_log(WLR_ERROR,
			"The theme uses deprecated alpha notation %s, please convert to "
			"#rrggbbaa to ensure your config works on newer labwc releases", hex);
	} else if (len == 9) {
		/* Inline alpha encoding like #aabbccff */
		rgba[3] = (hex_to_dec(hex[7]) * 16 + hex_to_dec(hex[8])) / 255.0;
	} else if (len > 7) {
		/* More than just #aabbcc */
		wlr_log(WLR_ERROR, "invalid alpha color encoding: '%s'", hex);
	}

	/* Pre-multiply everything as expected by wlr_scene */
	rgba[0] *= rgba[3];
	rgba[1] *= rgba[3];
	rgba[2] *= rgba[3];
}

static void
parse_hexstrs(const char *hexes, float colors[3][4])
{
	gchar **elements = g_strsplit(hexes, ",", -1);
	for (int i = 0; elements[i] && i < 3; i++) {
		parse_hexstr(elements[i], colors[i]);
	}
	g_strfreev(elements);
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

static enum lab_shape
parse_shape(const char *str)
{
	if (!strcasecmp(str, "Rectangle")) {
		return LAB_RECTANGLE;
	} else if (!strcasecmp(str, "Circle")) {
		return LAB_CIRCLE;
	} else {
		return LAB_RECTANGLE;
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
theme_builtin(struct theme *theme, struct server *server)
{
	theme->border_width = 1;
	theme->padding_height = 3;
	theme->title_height = INT_MIN;
	theme->menu_overlap_x = 0;
	theme->menu_overlap_y = 0;

	parse_hexstr("#e1dedb", theme->window_active_border_color);
	parse_hexstr("#f6f5f4", theme->window_inactive_border_color);

	parse_hexstr("#ff0000", theme->window_toggled_keybinds_color);

	parse_hexstr("#e1dedb", theme->window_active_title_bg_color);
	parse_hexstr("#f6f5f4", theme->window_inactive_title_bg_color);

	parse_hexstr("#000000", theme->window_active_label_text_color);
	parse_hexstr("#000000", theme->window_inactive_label_text_color);
	theme->window_label_text_justify = parse_justification("Center");
	theme->menu_title_text_justify = parse_justification("Center");

	theme->padding_width = 0;
	theme->window_button_width = 26;
	theme->window_button_spacing = 0;
	theme->window_button_hover_bg_shape = LAB_RECTANGLE;

	parse_hexstr("#000000",
		theme->window_active_button_menu_unpressed_image_color);
	parse_hexstr("#000000",
		theme->window_active_button_iconify_unpressed_image_color);
	parse_hexstr("#000000",
		theme->window_active_button_max_unpressed_image_color);
	parse_hexstr("#000000",
		theme->window_active_button_shade_unpressed_image_color);
	parse_hexstr("#000000",
		theme->window_active_button_omnipresent_unpressed_image_color);
	parse_hexstr("#000000",
		theme->window_active_button_close_unpressed_image_color);
	parse_hexstr("#000000",
		theme->window_inactive_button_menu_unpressed_image_color);
	parse_hexstr("#000000",
		theme->window_inactive_button_iconify_unpressed_image_color);
	parse_hexstr("#000000",
		theme->window_inactive_button_max_unpressed_image_color);
	parse_hexstr("#000000",
		theme->window_inactive_button_shade_unpressed_image_color);
	parse_hexstr("#000000",
		theme->window_inactive_button_omnipresent_unpressed_image_color);
	parse_hexstr("#000000",
		theme->window_inactive_button_close_unpressed_image_color);

	theme->window_active_shadow_size = 60;
	theme->window_inactive_shadow_size = 40;
	parse_hexstr("#00000060", theme->window_active_shadow_color);
	parse_hexstr("#00000040", theme->window_inactive_shadow_color);

	parse_hexstr("#fcfbfa", theme->menu_items_bg_color);
	parse_hexstr("#000000", theme->menu_items_text_color);
	parse_hexstr("#e1dedb", theme->menu_items_active_bg_color);
	parse_hexstr("#000000", theme->menu_items_active_text_color);

	theme->menu_item_padding_x = 7;
	theme->menu_item_padding_y = 4;

	theme->menu_min_width = 20;
	theme->menu_max_width = 200;

	theme->menu_separator_line_thickness = 1;
	theme->menu_separator_padding_width = 6;
	theme->menu_separator_padding_height = 3;
	parse_hexstr("#888888", theme->menu_separator_color);

	parse_hexstr("#589bda", theme->menu_title_bg_color);

	parse_hexstr("#ffffff", theme->menu_title_text_color);

	theme->osd_window_switcher_width = 600;
	theme->osd_window_switcher_width_is_percent = false;
	theme->osd_window_switcher_padding = 4;
	theme->osd_window_switcher_item_padding_x = 10;
	theme->osd_window_switcher_item_padding_y = 1;
	theme->osd_window_switcher_item_active_border_width = 2;

	/* inherit settings in post_processing() if not set elsewhere */
	theme->osd_window_switcher_preview_border_width = INT_MIN;
	zero_array(theme->osd_window_switcher_preview_border_color);
	theme->osd_window_switcher_preview_border_color[0][0] = FLT_MIN;

	theme->osd_workspace_switcher_boxes_width = 20;
	theme->osd_workspace_switcher_boxes_height = 20;

	/* inherit settings in post_processing() if not set elsewhere */
	theme->osd_bg_color[0] = FLT_MIN;
	theme->osd_border_width = INT_MIN;
	theme->osd_border_color[0] = FLT_MIN;
	theme->osd_label_text_color[0] = FLT_MIN;

	if (wlr_renderer_is_pixman(server->renderer)) {
		/* Draw only outlined overlay by default to save CPU resource */
		theme->snapping_overlay_region.bg_enabled = false;
		theme->snapping_overlay_edge.bg_enabled = false;
		theme->snapping_overlay_region.border_enabled = true;
		theme->snapping_overlay_edge.border_enabled = true;
	} else {
		theme->snapping_overlay_region.bg_enabled = true;
		theme->snapping_overlay_edge.bg_enabled = true;
		theme->snapping_overlay_region.border_enabled = false;
		theme->snapping_overlay_edge.border_enabled = false;
	}

	parse_hexstr("#8080b380", theme->snapping_overlay_region.bg_color);
	parse_hexstr("#8080b380", theme->snapping_overlay_edge.bg_color);

	/* inherit settings in post_processing() if not set elsewhere */
	theme->snapping_overlay_region.border_width = INT_MIN;
	theme->snapping_overlay_edge.border_width = INT_MIN;
	zero_array(theme->snapping_overlay_region.border_color);
	theme->snapping_overlay_region.border_color[0][0] = FLT_MIN;
	zero_array(theme->snapping_overlay_edge.border_color);
	theme->snapping_overlay_edge.border_color[0][0] = FLT_MIN;

	/* magnifier */
	parse_hexstr("#ff0000", theme->mag_border_color);
	theme->mag_border_width = 1;
}

static int
get_int_if_positive(const char *content, const char *field)
{
	int value = atoi(content);
	if (value < 0) {
		wlr_log(WLR_ERROR,
			"%s cannot be negative, clamping it to 0.", field);
		value = 0;
	}
	return value;
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
		theme->border_width = get_int_if_positive(
			value, "border.width");
	}
	if (match_glob(key, "padding.width")) {
		theme->padding_width = get_int_if_positive(
			value, "padding.width");
	}
	if (match_glob(key, "padding.height")) {
		theme->padding_height = get_int_if_positive(
			value, "padding.height");
	}
	if (match_glob(key, "titlebar.height")) {
		theme->title_height = get_int_if_positive(
			value, "titlebar.height");
	}
	if (match_glob(key, "menu.items.padding.x")) {
		theme->menu_item_padding_x = get_int_if_positive(
			value, "menu.items.padding.x");
	}
	if (match_glob(key, "menu.items.padding.y")) {
		theme->menu_item_padding_y = get_int_if_positive(
			value, "menu.items.padding.y");
	}
	if (match_glob(key, "menu.title.text.justify")) {
		theme->menu_title_text_justify = parse_justification(value);
	}
	if (match_glob(key, "menu.overlap.x")) {
		theme->menu_overlap_x = get_int_if_positive(
			value, "menu.overlap.x");
	}
	if (match_glob(key, "menu.overlap.y")) {
		theme->menu_overlap_y = get_int_if_positive(
			value, "menu.overlap.y");
	}

	if (match_glob(key, "window.active.border.color")) {
		parse_hexstr(value, theme->window_active_border_color);
	}
	if (match_glob(key, "window.inactive.border.color")) {
		parse_hexstr(value, theme->window_inactive_border_color);
	}
	/* border.color is obsolete, but handled for backward compatibility */
	if (match_glob(key, "border.color")) {
		parse_hexstr(value, theme->window_active_border_color);
		parse_hexstr(value, theme->window_inactive_border_color);
	}

	if (match_glob(key, "window.active.indicator.toggled-keybind.color")) {
		parse_hexstr(value, theme->window_toggled_keybinds_color);
	}

	if (match_glob(key, "window.active.title.bg.color")) {
		parse_hexstr(value, theme->window_active_title_bg_color);
	}
	if (match_glob(key, "window.inactive.title.bg.color")) {
		parse_hexstr(value, theme->window_inactive_title_bg_color);
	}

	if (match_glob(key, "window.active.label.text.color")) {
		parse_hexstr(value, theme->window_active_label_text_color);
	}
	if (match_glob(key, "window.inactive.label.text.color")) {
		parse_hexstr(value, theme->window_inactive_label_text_color);
	}
	if (match_glob(key, "window.label.text.justify")) {
		theme->window_label_text_justify = parse_justification(value);
	}

	if (match_glob(key, "window.button.width")) {
		theme->window_button_width = atoi(value);
		if (theme->window_button_width < 1) {
			wlr_log(WLR_ERROR, "window.button.width cannot "
				"be less than 1, clamping it to 1.");
			theme->window_button_width = 1;
		}
	}
	if (match_glob(key, "window.button.spacing")) {
		theme->window_button_spacing = get_int_if_positive(
			value, "window.button.spacing");
	}
	if (match_glob(key, "window.button.hover.bg.shape")) {
		theme->window_button_hover_bg_shape = parse_shape(value);
	}

	/* universal button */
	if (match_glob(key, "window.active.button.unpressed.image.color")) {
		parse_hexstr(value,
			theme->window_active_button_menu_unpressed_image_color);
		parse_hexstr(value,
			theme->window_active_button_iconify_unpressed_image_color);
		parse_hexstr(value,
			theme->window_active_button_max_unpressed_image_color);
		parse_hexstr(value,
			theme->window_active_button_shade_unpressed_image_color);
		parse_hexstr(value,
			theme->window_active_button_omnipresent_unpressed_image_color);
		parse_hexstr(value,
			theme->window_active_button_close_unpressed_image_color);
	}
	if (match_glob(key, "window.inactive.button.unpressed.image.color")) {
		parse_hexstr(value,
			theme->window_inactive_button_menu_unpressed_image_color);
		parse_hexstr(value,
			theme->window_inactive_button_iconify_unpressed_image_color);
		parse_hexstr(value,
			theme->window_inactive_button_max_unpressed_image_color);
		parse_hexstr(value,
			theme->window_inactive_button_shade_unpressed_image_color);
		parse_hexstr(value,
			theme->window_inactive_button_omnipresent_unpressed_image_color);
		parse_hexstr(value,
			theme->window_inactive_button_close_unpressed_image_color);
	}

	/* individual buttons */
	if (match_glob(key, "window.active.button.menu.unpressed.image.color")) {
		parse_hexstr(value,
			theme->window_active_button_menu_unpressed_image_color);
	}
	if (match_glob(key, "window.active.button.iconify.unpressed.image.color")) {
		parse_hexstr(value,
			theme->window_active_button_iconify_unpressed_image_color);
	}
	if (match_glob(key, "window.active.button.max.unpressed.image.color")) {
		parse_hexstr(value,
			theme->window_active_button_max_unpressed_image_color);
	}
	if (match_glob(key, "window.active.button.shade.unpressed.image.color")) {
		parse_hexstr(value,
			theme->window_active_button_shade_unpressed_image_color);
	}
	if (match_glob(key, "window.active.button.desk.unpressed.image.color")) {
		parse_hexstr(value,
			theme->window_active_button_omnipresent_unpressed_image_color);
	}
	if (match_glob(key, "window.active.button.close.unpressed.image.color")) {
		parse_hexstr(value,
			theme->window_active_button_close_unpressed_image_color);
	}
	if (match_glob(key, "window.inactive.button.menu.unpressed.image.color")) {
		parse_hexstr(value,
			theme->window_inactive_button_menu_unpressed_image_color);
	}
	if (match_glob(key, "window.inactive.button.iconify.unpressed.image.color")) {
		parse_hexstr(value,
			theme->window_inactive_button_iconify_unpressed_image_color);
	}
	if (match_glob(key, "window.inactive.button.max.unpressed.image.color")) {
		parse_hexstr(value,
			theme->window_inactive_button_max_unpressed_image_color);
	}
	if (match_glob(key, "window.inactive.button.shade.unpressed.image.color")) {
		parse_hexstr(value,
			theme->window_inactive_button_shade_unpressed_image_color);
	}
	if (match_glob(key, "window.inactive.button.desk.unpressed.image.color")) {
		parse_hexstr(value,
			theme->window_inactive_button_omnipresent_unpressed_image_color);
	}
	if (match_glob(key, "window.inactive.button.close.unpressed.image.color")) {
		parse_hexstr(value,
			theme->window_inactive_button_close_unpressed_image_color);
	}

	/* window drop-shadows */
	if (match_glob(key, "window.active.shadow.size")) {
		theme->window_active_shadow_size = get_int_if_positive(
			value, "window.active.shadow.size");
	}
	if (match_glob(key, "window.inactive.shadow.size")) {
		theme->window_inactive_shadow_size = get_int_if_positive(
			value, "window.inactive.shadow.size");
	}
	if (match_glob(key, "window.active.shadow.color")) {
		parse_hexstr(value, theme->window_active_shadow_color);
	}
	if (match_glob(key, "window.inactive.shadow.color")) {
		parse_hexstr(value, theme->window_inactive_shadow_color);
	}

	if (match_glob(key, "menu.width.min")) {
		theme->menu_min_width = get_int_if_positive(
			value, "menu.width.min");
	}
	if (match_glob(key, "menu.width.max")) {
		theme->menu_max_width = get_int_if_positive(
			value, "menu.width.max");
	}

	if (match_glob(key, "menu.items.bg.color")) {
		parse_hexstr(value, theme->menu_items_bg_color);
	}
	if (match_glob(key, "menu.items.text.color")) {
		parse_hexstr(value, theme->menu_items_text_color);
	}
	if (match_glob(key, "menu.items.active.bg.color")) {
		parse_hexstr(value, theme->menu_items_active_bg_color);
	}
	if (match_glob(key, "menu.items.active.text.color")) {
		parse_hexstr(value, theme->menu_items_active_text_color);
	}

	if (match_glob(key, "menu.separator.width")) {
		theme->menu_separator_line_thickness = get_int_if_positive(
			value, "menu.separator.width");
	}
	if (match_glob(key, "menu.separator.padding.width")) {
		theme->menu_separator_padding_width = get_int_if_positive(
			value, "menu.separator.padding.width");
	}
	if (match_glob(key, "menu.separator.padding.height")) {
		theme->menu_separator_padding_height = get_int_if_positive(
			value, "menu.separator.padding.height");
	}
	if (match_glob(key, "menu.separator.color")) {
		parse_hexstr(value, theme->menu_separator_color);
	}

	if (match_glob(key, "menu.title.bg.color")) {
		parse_hexstr(value, theme->menu_title_bg_color);
	}

	if (match_glob(key, "menu.title.text.color")) {
		parse_hexstr(value, theme->menu_title_text_color);
	}

	if (match_glob(key, "osd.bg.color")) {
		parse_hexstr(value, theme->osd_bg_color);
	}
	if (match_glob(key, "osd.border.width")) {
		theme->osd_border_width = get_int_if_positive(
			value, "osd.border.width");
	}
	if (match_glob(key, "osd.border.color")) {
		parse_hexstr(value, theme->osd_border_color);
	}
	if (match_glob(key, "osd.window-switcher.width")) {
		if (strrchr(value, '%')) {
			theme->osd_window_switcher_width_is_percent = true;
		} else {
			theme->osd_window_switcher_width_is_percent = false;
		}
		theme->osd_window_switcher_width = get_int_if_positive(
			value, "osd.window-switcher.width");
	}
	if (match_glob(key, "osd.window-switcher.padding")) {
		theme->osd_window_switcher_padding = get_int_if_positive(
			value, "osd.window-switcher.padding");
	}
	if (match_glob(key, "osd.window-switcher.item.padding.x")) {
		theme->osd_window_switcher_item_padding_x =
			get_int_if_positive(
				value, "osd.window-switcher.item.padding.x");
	}
	if (match_glob(key, "osd.window-switcher.item.padding.y")) {
		theme->osd_window_switcher_item_padding_y =
			get_int_if_positive(
				value, "osd.window-switcher.item.padding.y");
	}
	if (match_glob(key, "osd.window-switcher.item.active.border.width")) {
		theme->osd_window_switcher_item_active_border_width =
			get_int_if_positive(
				value, "osd.window-switcher.item.active.border.width");
	}
	if (match_glob(key, "osd.window-switcher.preview.border.width")) {
		theme->osd_window_switcher_preview_border_width =
			get_int_if_positive(
				value, "osd.window-switcher.preview.border.width");
	}
	if (match_glob(key, "osd.window-switcher.preview.border.color")) {
		parse_hexstrs(value, theme->osd_window_switcher_preview_border_color);
	}
	if (match_glob(key, "osd.workspace-switcher.boxes.width")) {
		theme->osd_workspace_switcher_boxes_width =
			get_int_if_positive(
				value, "osd.workspace-switcher.boxes.width");
	}
	if (match_glob(key, "osd.workspace-switcher.boxes.height")) {
		theme->osd_workspace_switcher_boxes_height =
			get_int_if_positive(
				value, "osd.workspace-switcher.boxes.height");
	}
	if (match_glob(key, "osd.label.text.color")) {
		parse_hexstr(value, theme->osd_label_text_color);
	}
	if (match_glob(key, "snapping.overlay.region.bg.enabled")) {
		set_bool(value, &theme->snapping_overlay_region.bg_enabled);
	}
	if (match_glob(key, "snapping.overlay.edge.bg.enabled")) {
		set_bool(value, &theme->snapping_overlay_edge.bg_enabled);
	}
	if (match_glob(key, "snapping.overlay.region.border.enabled")) {
		set_bool(value, &theme->snapping_overlay_region.border_enabled);
	}
	if (match_glob(key, "snapping.overlay.edge.border.enabled")) {
		set_bool(value, &theme->snapping_overlay_edge.border_enabled);
	}
	if (match_glob(key, "snapping.overlay.region.bg.color")) {
		parse_hexstr(value, theme->snapping_overlay_region.bg_color);
	}
	if (match_glob(key, "snapping.overlay.edge.bg.color")) {
		parse_hexstr(value, theme->snapping_overlay_edge.bg_color);
	}
	if (match_glob(key, "snapping.overlay.region.border.width")) {
		theme->snapping_overlay_region.border_width = get_int_if_positive(
			value, "snapping.overlay.region.border.width");
	}
	if (match_glob(key, "snapping.overlay.edge.border.width")) {
		theme->snapping_overlay_edge.border_width = get_int_if_positive(
			value, "snapping.overlay.edge.border.width");
	}
	if (match_glob(key, "snapping.overlay.region.border.color")) {
		parse_hexstrs(value, theme->snapping_overlay_region.border_color);
	}
	if (match_glob(key, "snapping.overlay.edge.border.color")) {
		parse_hexstrs(value, theme->snapping_overlay_edge.border_color);
	}

	if (match_glob(key, "magnifier.border.width")) {
		theme->mag_border_width = get_int_if_positive(
			value, "magnifier.border.width");
	}
	if (match_glob(key, "magnifier.border.color")) {
		parse_hexstr(value, theme->mag_border_color);
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
	 * overlap each other (as their line-thicknesses are greater than the
	 * line-thickness of the arc). As a result, there is no inner rounded
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
	int corner_width = ssd_get_corner_width();

	struct wlr_box box = {
		.x = 0,
		.y = 0,
		.width = corner_width + theme->border_width,
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

/*
 * Draw the buffer used to render the edges of window drop-shadows. The buffer
 * is 1 pixel tall and `visible_size` pixels wide and can be rotated and scaled for the
 * different edges.  The buffer is drawn as would be found at the right-hand
 * edge of a window. The gradient has a color of `start_color` at its left edge
 * fading to clear at its right edge.
 */
static void
shadow_edge_gradient(struct lab_data_buffer *buffer,
		int visible_size, int total_size, float start_color[4])
{
	if (!buffer) {
		/* This type of shadow is disabled, do nothing */
		return;
	}

	assert(buffer->format == DRM_FORMAT_ARGB8888);
	uint8_t *pixels = buffer->data;

	/* Inset portion which is obscured */
	int inset = total_size - visible_size;

	/* Standard deviation normalised against the shadow width, squared */
	double variance = 0.3 * 0.3;

	for (int x = 0; x < visible_size; x++) {
		/*
		 * x normalised against total shadow width. We add on inset here
		 * because we don't bother drawing inset for the edge shadow
		 * buffers but still need the pattern to line up with the corner
		 * shadow buffers which do have inset drawn.
		 */
		double xn = (double)(x + inset) / (double)total_size;

		/* Gausian dropoff */
		double alpha = exp(-(xn * xn) / variance);

		/* RGBA values are all pre-multiplied */
		pixels[4 * x] = start_color[2] * alpha * 255;
		pixels[4 * x + 1] = start_color[1] * alpha * 255;
		pixels[4 * x + 2] = start_color[0] * alpha * 255;
		pixels[4 * x + 3] = start_color[3] * alpha * 255;
	}
}

/*
 * Draw the buffer used to render the corners of window drop-shadows.  The
 * shadow looks better if the buffer is inset behind the window, so the buffer
 * is square with a size of radius+inset.  The buffer is drawn for the
 * bottom-right corner but can be rotated for other corners.  The gradient fades
 * from `start_color` at the top-left to clear at the opposite edge.
 *
 * If the window is translucent we don't want the shadow to be visible through
 * it.  For the bottom corners of the window this is easy, we just erase the
 * square of the buffer which will be behind the window.  For the top it's a
 * little more complicated because the titlebar can have rounded corners.
 * However, the titlebar itself is always opaque so we only have to erase the
 * L-shaped area of the buffer which can appear behind the non-titlebar part of
 * the window.
 */
static void
shadow_corner_gradient(struct lab_data_buffer *buffer, int visible_size,
	int total_size, int titlebar_height, float start_color[4])
{
	if (!buffer) {
		/* This type of shadow is disabled, do nothing */
		return;
	}

	assert(buffer->format == DRM_FORMAT_ARGB8888);
	uint8_t *pixels = buffer->data;

	/* Standard deviation normalised against the shadow width, squared */
	double variance = 0.3 * 0.3;

	int inset = total_size - visible_size;

	for (int y = 0; y < total_size; y++) {
		uint8_t *pixel_row = &pixels[y * buffer->stride];
		for (int x = 0; x < total_size; x++) {
			/* x and y normalised against total shadow width */
			double x_norm = (double)(x) / (double)total_size;
			double y_norm = (double)(y) / (double)total_size;
			/*
			 * For Gaussian drop-off in 2d you can just calculate
			 * the outer product of the horizontal and vertical
			 * profiles.
			 */
			double gauss_x = exp(-(x_norm * x_norm) / variance);
			double gauss_y = exp(-(y_norm * y_norm) / variance);
			double alpha = gauss_x * gauss_y;

			/*
			 * Erase the L-shaped region which could be visible
			 * through a transparent window but not obscured by the
			 * titlebar. If inset is smaller than the titlebar
			 * height then there's nothing to do, this is handled by
			 * (inset - titlebar_height) being negative.
			 */
			bool in1 = x < inset && y < inset - titlebar_height;
			bool in2 = x < inset - titlebar_height && y < inset;
			if (in1 || in2) {
				alpha = 0.0;
			}

			/* RGBA values are all pre-multiplied */
			pixel_row[4 * x] = start_color[2] * alpha * 255;
			pixel_row[4 * x + 1] = start_color[1] * alpha * 255;
			pixel_row[4 * x + 2] = start_color[0] * alpha * 255;
			pixel_row[4 * x + 3] = start_color[3] * alpha * 255;
		}
	}
}

static void
create_shadows(struct theme *theme)
{
	/* Size of shadow visible extending beyond the window */
	int visible_active_size = theme->window_active_shadow_size;
	int visible_inactive_size = theme->window_inactive_shadow_size;
	/* How far inside the window the shadow inset begins */
	int inset_active = (double)visible_active_size * SSD_SHADOW_INSET;
	int inset_inactive = (double)visible_inactive_size * SSD_SHADOW_INSET;
	/* Total width including visible and obscured portion */
	int total_active_size = visible_active_size + inset_active;
	int total_inactive_size = visible_inactive_size + inset_inactive;

	/*
	 * Edge shadows don't need to be inset so the buffers are sized just for
	 * the visible width.  Corners are inset so the buffers are larger for
	 * this.
	 */
	if (visible_active_size > 0) {
		theme->shadow_edge_active = buffer_create_cairo(
			visible_active_size, 1, 1.0, true);
		theme->shadow_corner_top_active = buffer_create_cairo(
			total_active_size, total_active_size, 1.0, true);
		theme->shadow_corner_bottom_active = buffer_create_cairo(
			total_active_size, total_active_size, 1.0, true);
		if (!theme->shadow_corner_top_active
				|| !theme->shadow_corner_bottom_active
				|| !theme->shadow_edge_active) {
			wlr_log(WLR_ERROR, "Failed to allocate shadow buffer");
			return;
		}
	}
	if (visible_inactive_size > 0) {
		theme->shadow_edge_inactive = buffer_create_cairo(
			visible_inactive_size, 1, 1.0, true);
		theme->shadow_corner_top_inactive = buffer_create_cairo(
			total_inactive_size, total_inactive_size, 1.0, true);
		theme->shadow_corner_bottom_inactive = buffer_create_cairo(
			total_inactive_size, total_inactive_size, 1.0, true);
		if (!theme->shadow_corner_top_inactive
				|| !theme->shadow_corner_bottom_inactive
				|| !theme->shadow_edge_inactive) {
			wlr_log(WLR_ERROR, "Failed to allocate shadow buffer");
			return;
		}
	}

	shadow_edge_gradient(theme->shadow_edge_active, visible_active_size,
		total_active_size, theme->window_active_shadow_color);
	shadow_edge_gradient(theme->shadow_edge_inactive, visible_inactive_size,
		total_inactive_size, theme->window_inactive_shadow_color);
	shadow_corner_gradient(theme->shadow_corner_top_active,
		visible_active_size, total_active_size,
		theme->title_height, theme->window_active_shadow_color);
	shadow_corner_gradient(theme->shadow_corner_bottom_active,
		visible_active_size, total_active_size, 0,
		theme->window_active_shadow_color);
	shadow_corner_gradient(theme->shadow_corner_top_inactive,
		visible_inactive_size, total_inactive_size,
		theme->title_height, theme->window_inactive_shadow_color);
	shadow_corner_gradient(theme->shadow_corner_bottom_inactive,
		visible_inactive_size, total_inactive_size, 0,
		theme->window_inactive_shadow_color);
}

static void
fill_colors_with_osd_theme(struct theme *theme, float colors[3][4])
{
	memcpy(colors[0], theme->osd_bg_color, sizeof(colors[0]));
	memcpy(colors[1], theme->osd_label_text_color, sizeof(colors[1]));
	memcpy(colors[2], theme->osd_bg_color, sizeof(colors[2]));
}

static void
post_processing(struct theme *theme)
{
	int h = MAX(font_height(&rc.font_activewindow), font_height(&rc.font_inactivewindow));
	if (theme->title_height < h) {
		theme->title_height = h + 2 * theme->padding_height;
	}

	theme->menu_item_height = font_height(&rc.font_menuitem)
		+ 2 * theme->menu_item_padding_y;

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
	if (theme->osd_window_switcher_width_is_percent) {
		theme->osd_window_switcher_width =
			MIN(theme->osd_window_switcher_width, 100);
	}
	if (theme->osd_window_switcher_preview_border_width == INT_MIN) {
		theme->osd_window_switcher_preview_border_width =
			theme->osd_border_width;
	}
	if (theme->osd_window_switcher_preview_border_color[0][0] == FLT_MIN) {
		fill_colors_with_osd_theme(theme,
			theme->osd_window_switcher_preview_border_color);
	}

	if (theme->snapping_overlay_region.border_width == INT_MIN) {
		theme->snapping_overlay_region.border_width =
			theme->osd_border_width;
	}
	if (theme->snapping_overlay_edge.border_width == INT_MIN) {
		theme->snapping_overlay_edge.border_width =
			theme->osd_border_width;
	}
	if (theme->snapping_overlay_region.border_color[0][0] == FLT_MIN) {
		fill_colors_with_osd_theme(theme,
			theme->snapping_overlay_region.border_color);
	}
	if (theme->snapping_overlay_edge.border_color[0][0] == FLT_MIN) {
		fill_colors_with_osd_theme(theme,
			theme->snapping_overlay_edge.border_color);
	}
}

void
theme_init(struct theme *theme, struct server *server, const char *theme_name)
{
	/*
	 * Set some default values. This is particularly important on
	 * reconfigure as not all themes set all options
	 */
	theme_builtin(theme, server);

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
	create_shadows(theme);
}

void
theme_finish(struct theme *theme)
{
	zdrop(&theme->button_close_active_unpressed);
	zdrop(&theme->button_maximize_active_unpressed);
	zdrop(&theme->button_restore_active_unpressed);
	zdrop(&theme->button_shade_active_unpressed);
	zdrop(&theme->button_unshade_active_unpressed);
	zdrop(&theme->button_omnipresent_active_unpressed);
	zdrop(&theme->button_exclusive_active_unpressed);
	zdrop(&theme->button_iconify_active_unpressed);
	zdrop(&theme->button_menu_active_unpressed);

	zdrop(&theme->button_close_inactive_unpressed);
	zdrop(&theme->button_maximize_inactive_unpressed);
	zdrop(&theme->button_restore_inactive_unpressed);
	zdrop(&theme->button_shade_inactive_unpressed);
	zdrop(&theme->button_unshade_inactive_unpressed);
	zdrop(&theme->button_omnipresent_inactive_unpressed);
	zdrop(&theme->button_exclusive_inactive_unpressed);
	zdrop(&theme->button_iconify_inactive_unpressed);
	zdrop(&theme->button_menu_inactive_unpressed);

	zdrop(&theme->button_close_active_hover);
	zdrop(&theme->button_maximize_active_hover);
	zdrop(&theme->button_restore_active_hover);
	zdrop(&theme->button_shade_active_hover);
	zdrop(&theme->button_unshade_active_hover);
	zdrop(&theme->button_omnipresent_active_hover);
	zdrop(&theme->button_exclusive_active_hover);
	zdrop(&theme->button_iconify_active_hover);
	zdrop(&theme->button_menu_active_hover);

	zdrop(&theme->button_close_inactive_hover);
	zdrop(&theme->button_maximize_inactive_hover);
	zdrop(&theme->button_restore_inactive_hover);
	zdrop(&theme->button_shade_inactive_hover);
	zdrop(&theme->button_unshade_inactive_hover);
	zdrop(&theme->button_omnipresent_inactive_hover);
	zdrop(&theme->button_exclusive_inactive_hover);
	zdrop(&theme->button_iconify_inactive_hover);
	zdrop(&theme->button_menu_inactive_hover);

	zdrop(&theme->corner_top_left_active_normal);
	zdrop(&theme->corner_top_left_inactive_normal);
	zdrop(&theme->corner_top_right_active_normal);
	zdrop(&theme->corner_top_right_inactive_normal);

	zdrop(&theme->shadow_corner_top_active);
	zdrop(&theme->shadow_corner_bottom_active);
	zdrop(&theme->shadow_edge_active);
	zdrop(&theme->shadow_corner_top_inactive);
	zdrop(&theme->shadow_corner_bottom_inactive);
	zdrop(&theme->shadow_edge_inactive);
}
