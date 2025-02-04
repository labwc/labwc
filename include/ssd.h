/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SSD_H
#define LABWC_SSD_H

#include <wayland-server-core.h>
#include "common/border.h"

struct wlr_cursor;

/*
 * Shadows should start at a point inset from the actual window border, see
 * discussion on https://github.com/labwc/labwc/pull/1648.  This constant
 * specifies inset as a multiple of visible shadow size.
 */
#define SSD_SHADOW_INSET 0.3

/*
 * Sequence these according to the order they should be processed for
 * press and hover events. Bear in mind that some of their respective
 * interactive areas overlap, so for example buttons need to come before title.
 */
enum ssd_part_type {
	LAB_SSD_NONE = 0,

	LAB_SSD_BUTTON_CLOSE = 1,
	LAB_SSD_BUTTON_MAXIMIZE,
	LAB_SSD_BUTTON_ICONIFY,
	LAB_SSD_BUTTON_WINDOW_ICON,
	LAB_SSD_BUTTON_WINDOW_MENU,
	LAB_SSD_BUTTON_SHADE,
	LAB_SSD_BUTTON_OMNIPRESENT,
	/* only for internal use */
	LAB_SSD_BUTTON_FIRST = LAB_SSD_BUTTON_CLOSE,
	LAB_SSD_BUTTON_LAST = LAB_SSD_BUTTON_OMNIPRESENT,
	LAB_SSD_BUTTON,

	LAB_SSD_PART_TITLEBAR,
	LAB_SSD_PART_TITLEBAR_CORNER_RIGHT,
	LAB_SSD_PART_TITLEBAR_CORNER_LEFT,
	LAB_SSD_PART_TITLE,

	/* shared by shadows, borders and extents */
	LAB_SSD_PART_CORNER_TOP_LEFT,
	LAB_SSD_PART_CORNER_TOP_RIGHT,
	LAB_SSD_PART_CORNER_BOTTOM_RIGHT,
	LAB_SSD_PART_CORNER_BOTTOM_LEFT,
	LAB_SSD_PART_TOP,
	LAB_SSD_PART_RIGHT,
	LAB_SSD_PART_BOTTOM,
	LAB_SSD_PART_LEFT,

	LAB_SSD_CLIENT,
	LAB_SSD_FRAME,
	LAB_SSD_ROOT,
	LAB_SSD_MENU,
	LAB_SSD_OSD,
	LAB_SSD_LAYER_SURFACE,
	LAB_SSD_LAYER_SUBSURFACE,
	LAB_SSD_UNMANAGED,
	LAB_SSD_ALL,
	LAB_SSD_END_MARKER
};

enum ssd_mode {
	LAB_SSD_MODE_INVALID,
	LAB_SSD_MODE_NONE,
	LAB_SSD_MODE_BORDER,
	LAB_SSD_MODE_FULL,
};

/* Forward declare arguments */
struct ssd;
struct ssd_button;
struct ssd_hover_state;
struct view;
struct wlr_scene;
struct wlr_scene_node;

/*
 * Public SSD API
 *
 * For convenience in dealing with non-SSD views, this API allows NULL
 * ssd/button/node arguments and attempts to do something sensible in
 * that case (e.g. no-op/return default values).
 *
 * NULL scene/view arguments are not allowed.
 */
struct ssd *ssd_create(struct view *view, bool active);
struct border ssd_get_margin(const struct ssd *ssd);
int ssd_get_corner_width(void);
void ssd_update_margin(struct ssd *ssd);
void ssd_set_active(struct ssd *ssd, bool active);
void ssd_update_title(struct ssd *ssd);
void ssd_update_geometry(struct ssd *ssd);
void ssd_destroy(struct ssd *ssd);
void ssd_set_titlebar(struct ssd *ssd, bool enabled);
void ssd_update_window_icon(struct ssd *ssd);

void ssd_enable_keybind_inhibit_indicator(struct ssd *ssd, bool enable);
void ssd_enable_shade(struct ssd *ssd, bool enable);

struct ssd_hover_state *ssd_hover_state_new(void);
void ssd_update_button_hover(struct wlr_scene_node *node,
	struct ssd_hover_state *hover_state);

enum ssd_part_type ssd_button_get_type(const struct ssd_button *button);
struct view *ssd_button_get_view(const struct ssd_button *button);

/* Public SSD helpers */
enum ssd_part_type ssd_get_part_type(const struct ssd *ssd,
	struct wlr_scene_node *node, struct wlr_cursor *cursor);
uint32_t ssd_resize_edges(enum ssd_part_type type);
bool ssd_part_contains(enum ssd_part_type whole, enum ssd_part_type candidate);
enum ssd_mode ssd_mode_parse(const char *mode);

/* TODO: clean up / update */
struct border ssd_thickness(struct view *view);
struct wlr_box ssd_max_extents(struct view *view);

/* SSD debug helpers */
bool ssd_debug_is_root_node(const struct ssd *ssd, struct wlr_scene_node *node);
const char *ssd_debug_get_node_name(const struct ssd *ssd,
	struct wlr_scene_node *node);

#endif /* LABWC_SSD_H */
