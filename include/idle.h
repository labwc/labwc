/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_IDLE_H
#define LABWC_IDLE_H

struct wl_display;
struct wlr_seat;

void idle_manager_create(struct wl_display *display, struct wlr_seat *wlr_seat);
void idle_manager_notify_activity(struct wlr_seat *seat);

#endif /* LABWC_IDLE_H */
