/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_XBM_H
#define LABWC_XBM_H

#include <wlr/render/wlr_renderer.h>

#include "xbm/parse.h"

/**
 * xbm_load - load theme xbm files into global theme struct
 */
void xbm_load(struct theme *theme);

#endif /* LABWC_XBM_H */
