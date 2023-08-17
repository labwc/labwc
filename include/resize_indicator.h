/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_RESIZE_INDICATOR_H
#define LABWC_RESIZE_INDICATOR_H

struct server;
struct view;

enum resize_indicator_mode {
	LAB_RESIZE_INDICATOR_NEVER = 0,
	LAB_RESIZE_INDICATOR_ALWAYS,
	LAB_RESIZE_INDICATOR_NON_PIXEL
};

void resize_indicator_reconfigure(struct server *server);
void resize_indicator_show(struct view *view);
void resize_indicator_update(struct view *view);
void resize_indicator_hide(struct view *view);

#endif /* LABWC_RESIZE_INDICATOR_H */
