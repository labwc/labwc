/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SSD_H
#define LABWC_SSD_H

#include "common/border.h"
#include "common/node-type.h"
#include "config/types.h"

enum ssd_active_state {
	SSD_INACTIVE = 0,
	SSD_ACTIVE = 1,
};

#define FOR_EACH_ACTIVE_STATE(active) for (active = SSD_INACTIVE; active <= SSD_ACTIVE; active++)

struct wlr_cursor;

/*
 * Shadows should start at a point inset from the actual window border, see
 * discussion on https://github.com/labwc/labwc/pull/1648.  This constant
 * specifies inset as a multiple of visible shadow size.
 */
#define SSD_SHADOW_INSET 0.3

/* Forward declare arguments */
struct server;
struct ssd;
struct ssd_part;
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

void ssd_enable_keybind_inhibit_indicator(struct ssd *ssd, bool enable);
void ssd_enable_shade(struct ssd *ssd, bool enable);

void ssd_update_hovered_button(struct server *server,
	struct wlr_scene_node *node);

enum lab_node_type ssd_part_get_type(const struct ssd_part *part);
struct view *ssd_part_get_view(const struct ssd_part *part);

/* Public SSD helpers */

/*
 * Returns a part type that represents a mouse context like "Top", "Left" and
 * "TRCorner" when the cursor is on the window border or resizing handle.
 */
enum lab_node_type ssd_get_resizing_type(const struct ssd *ssd,
	struct wlr_cursor *cursor);
enum lab_ssd_mode ssd_mode_parse(const char *mode);

/* TODO: clean up / update */
struct border ssd_thickness(struct view *view);
struct wlr_box ssd_max_extents(struct view *view);

/* SSD debug helpers */
bool ssd_debug_is_root_node(const struct ssd *ssd, struct wlr_scene_node *node);
const char *ssd_debug_get_node_name(const struct ssd *ssd,
	struct wlr_scene_node *node);

#endif /* LABWC_SSD_H */
