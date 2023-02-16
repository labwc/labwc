/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_REGIONS_H
#define __LABWC_REGIONS_H

#include <wlr/util/box.h>

struct seat;
struct view;
struct server;
struct output;
struct wl_list;
struct wlr_box;
struct multi_rect;

/* Double use: rcxml.c for config and output.c for usage */
struct region {
	struct wl_list link; /* struct rcxml.regions, struct output.regions */
	struct output *output;
	char *name;
	struct wlr_box geo;
	struct wlr_box percentage;
	struct {
		int x;
		int y;
	} center;
};

struct region_overlay {
	struct wlr_scene_tree *tree;
	union {
		struct wlr_scene_rect *overlay;
		struct multi_rect *pixman_overlay;
	};
};

/* Can be used as a cheap check to detect if there are any regions configured */
bool regions_available(void);

/**
 * regions_reconfigure*() - re-initializes all regions from struct rc.
 *
 * - all views are evacuated from the given output (or all of them)
 * - all output local regions are destroyed
 * - new output local regions are created from struct rc
 * - the region geometry is re-calculated
 */
void regions_reconfigure(struct server *server);
void regions_reconfigure_output(struct output *output);

/* re-calculate the geometry based on usable area */
void regions_update_geometry(struct output *output);

/**
 * Mark all views which are currently region-tiled to the given output as
 * evacuated. This means that the view->tiled_region pointer is reset to
 * NULL but view->tiled_region_evacuate is set to a copy of the region name.
 *
 * The next time desktop_arrange_all_views() causes a call to
 * view_apply_region_geometry() it will try to find a new output and then
 * search for a region with the same name. If found, view->tiled_region will
 * be set to the new region and view->tiled_region_evacuate will be free'd.
 *
 * If no region with the old name is found (e.g. the user deleted or renamed
 * the region in rc.xml and caused a Reconfigure) the view will be reset to
 * non-tiled state and view->tiled_region_evacuate will be free'd.
 */
void regions_evacuate_output(struct output *output);

/* Free all regions in given wl_list pointer */
void regions_destroy(struct seat *seat, struct wl_list *regions);

/* Get output local region from cursor or name, may be NULL */
struct region *regions_from_cursor(struct server *server);
struct region *regions_from_name(const char *region_name, struct output *output);

void regions_show_overlay(struct view *view, struct seat *seat, struct region *region);
void regions_hide_overlay(struct seat *seat);

#endif /* __LABWC_REGIONS_H */
