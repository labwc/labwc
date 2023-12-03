/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_DND_H
#define LABWC_DND_H

#include <wayland-server-core.h>

struct seat;

void dnd_init(struct seat *seat);
void dnd_icons_show(struct seat *seat, bool show);
void dnd_icons_move(struct seat *seat, double x, double y);
void dnd_finish(struct seat *seat);

#endif /* LABWC_DND_H */
