// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <wlr/types/wlr_scene.h>

struct wlr_scene_rect *
lab_wlr_scene_get_rect(struct wlr_scene_node *node)
{
	assert(node->type == WLR_SCENE_NODE_RECT);
	return (struct wlr_scene_rect *)node;
}
