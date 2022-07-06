// SPDX-License-Identifier: GPL-2.0-only

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <float.h>
#include <string.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include "common/graphic-helpers.h"
#include "common/mem.h"
#include "labwc.h"
#include "regions.h"

void
regions_init(struct server *server, struct seat *seat)
{
	/* To be filled later */
}

void
regions_update(struct output *output)
{
	/* To be filled later */
}

void
regions_destroy(struct wl_list *regions)
{
	assert(regions);
	struct region *region, *region_tmp;
	wl_list_for_each_safe(region, region_tmp, regions, link) {
		wl_list_remove(&region->link);
		zfree(region->name);
		zfree(region);
	}
}

