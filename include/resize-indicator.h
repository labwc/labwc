/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_RESIZE_INDICATOR_H
#define LABWC_RESIZE_INDICATOR_H

struct server;
struct view;

void resize_indicator_reconfigure(struct server *server);
void resize_indicator_show(struct view *view);
void resize_indicator_update(struct view *view);
void resize_indicator_hide(struct view *view);

#endif /* LABWC_RESIZE_INDICATOR_H */
