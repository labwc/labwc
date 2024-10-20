/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_DIRECTION_H
#define LABWC_DIRECTION_H

#include "view.h"

enum wlr_direction direction_from_view_edge(enum view_edge edge);
enum wlr_direction direction_get_opposite(enum wlr_direction direction);

#endif /* LABWC_DIRECTION_H */
