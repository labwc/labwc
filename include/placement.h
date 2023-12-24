/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_PLACEMENT_H
#define LABWC_PLACEMENT_H

#include <stdbool.h>
#include "view.h"

bool placement_find_best(struct view *view, int *x, int *y);

#endif /* LABWC_PLACEMENT_H */
