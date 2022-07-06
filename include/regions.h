/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_REGIONS_H
#define __LABWC_REGIONS_H

//FIXME: ignore that there could be more than a single seat

struct seat;
struct view;
struct server;
struct output;
struct wl_list;
struct wlr_box;

/* Double use: rcxml.c for config and output.c for usage */
struct region {
	struct wl_list link; /* struct rcxml.regions, struct output.regions */
	char *name;
	struct wlr_box geo;
	struct wlr_box percentage;
	struct {
		int x;
		int y;
	} center;
};

/* Can be used as a cheap check to detect if there are any regions configured */
bool regions_available(void);

void regions_init(struct server *server, struct seat *seat);
void regions_update(struct output *output);
void regions_destroy(struct wl_list *regions);

#endif /* __LABWC_REGIONS_H */
