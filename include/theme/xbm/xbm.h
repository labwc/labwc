#ifndef XBM_H
#define XBM_H

#include <wlr/render/wlr_renderer.h>

#include "theme.h"
#include "theme/xbm/parse.h"

/**
 * xbm_load - load theme xbm files into global theme struct
 */
void xbm_load(struct wlr_renderer *renderer);

#endif /* XBM_H */
