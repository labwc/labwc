// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/render/swapchain.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/render/allocator.h>
#include "config/rcxml.h"
#include "common/array.h"
#include "common/box.h"
#include "common/lab-scene-rect.h"
#include "labwc.h"
#include "osd.h"
#include "output.h"
#include "scaled-buffer/scaled-font-buffer.h"
#include "scaled-buffer/scaled-icon-buffer.h"
#include "theme.h"
#include "view.h"

struct osd_thumbnail_scene_item {
	struct view *view;
	struct wlr_scene_tree *tree;
	struct scaled_font_buffer *normal_title;
	struct scaled_font_buffer *active_title;
	struct lab_scene_rect *active_bg;
};

static void
render_node(struct server *server, struct wlr_render_pass *pass,
		struct wlr_scene_node *node, int x, int y)
{
	switch (node->type) {
	case WLR_SCENE_NODE_TREE: {
		struct wlr_scene_tree *tree = wlr_scene_tree_from_node(node);
		struct wlr_scene_node *child;
		wl_list_for_each(child, &tree->children, link) {
			render_node(server, pass, child, x + node->x, y + node->y);
		}
		break;
	}
	case WLR_SCENE_NODE_BUFFER: {
		struct wlr_scene_buffer *scene_buffer =
			wlr_scene_buffer_from_node(node);
		if (!scene_buffer->buffer) {
			break;
		}
		struct wlr_texture *texture = wlr_texture_from_buffer(
			server->renderer, scene_buffer->buffer);
		if (!texture) {
			break;
		}
		wlr_render_pass_add_texture(pass, &(struct wlr_render_texture_options){
			.texture = texture,
			.src_box = scene_buffer->src_box,
			.dst_box = {
				.x = x,
				.y = y,
				.width = scene_buffer->dst_width,
				.height = scene_buffer->dst_height,
			},
			.transform = scene_buffer->transform,
		});
		wlr_texture_destroy(texture);
		break;
	}
	case WLR_SCENE_NODE_RECT:
		/* should be unreached */
		wlr_log(WLR_ERROR, "ignoring rect");
		break;
	}
}

static struct wlr_buffer *
render_thumb(struct output *output, struct view *view)
{
	struct server *server = output->server;
	struct wlr_buffer *buffer = wlr_allocator_create_buffer(server->allocator,
		view->current.width, view->current.height,
		&output->wlr_output->swapchain->format);
	struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(
		server->renderer, buffer, NULL);
	render_node(server, pass, &view->content_tree->node, 0, 0);
	if (!wlr_render_pass_submit(pass)) {
		wlr_log(WLR_ERROR, "failed to submit render pass");
		wlr_buffer_drop(buffer);
		return NULL;
	}
	return buffer;
}

static struct scaled_font_buffer *
create_title(struct wlr_scene_tree *parent,
		struct window_switcher_thumbnail_theme *switcher_theme,
		const char *title, const float *title_color,
		const float *bg_color, int y)
{
	struct scaled_font_buffer *buffer =
		scaled_font_buffer_create(parent);
	scaled_font_buffer_update(buffer, title,
		switcher_theme->item_width - 2 * switcher_theme->item_padding,
		&rc.font_osd, title_color, bg_color);
	wlr_scene_node_set_position(&buffer->scene_buffer->node,
		(switcher_theme->item_width - buffer->width) / 2, y);
	return buffer;
}

static struct osd_thumbnail_scene_item *
create_item_scene(struct wlr_scene_tree *parent, struct view *view,
		struct output *output)
{
	struct server *server = output->server;
	struct theme *theme = server->theme;
	struct window_switcher_thumbnail_theme *switcher_theme =
		&theme->osd_window_switcher_thumbnail;
	int padding = theme->border_width + switcher_theme->item_padding;
	int title_y = switcher_theme->item_height - padding - switcher_theme->title_height;
	struct wlr_box thumb_bounds = {
		.x = padding,
		.y = padding,
		.width = switcher_theme->item_width - 2 * padding,
		.height = title_y - 2 * padding,
	};
	if (thumb_bounds.width <= 0 || thumb_bounds.height <= 0) {
		wlr_log(WLR_ERROR, "too small thumbnail area");
		return NULL;
	}

	struct osd_thumbnail_scene_item *item =
		wl_array_add(&output->osd_scene.items, sizeof(*item));
	item->tree = wlr_scene_tree_create(parent);
	item->view = view;

	/* background for selected item */
	struct lab_scene_rect_options opts = {
		.width = switcher_theme->item_width,
		.height = switcher_theme->item_height,
		.bg_color = switcher_theme->item_active_bg_color,
		.nr_borders = 1,
		.border_colors = (float *[1]) { switcher_theme->item_active_border_color },
		.border_width = switcher_theme->item_active_border_width,
	};
	item->active_bg = lab_scene_rect_create(item->tree, &opts);

	/* thumbnail */
	struct wlr_buffer *thumb_buffer = render_thumb(output, view);
	if (thumb_buffer) {
		struct wlr_scene_buffer *thumb_scene_buffer =
			wlr_scene_buffer_create(item->tree, thumb_buffer);
		wlr_buffer_drop(thumb_buffer);
		struct wlr_box thumb_box = box_fit_within(
			thumb_buffer->width, thumb_buffer->height,
			&thumb_bounds);
		wlr_scene_buffer_set_dest_size(thumb_scene_buffer,
			thumb_box.width, thumb_box.height);
		wlr_scene_node_set_position(&thumb_scene_buffer->node,
			thumb_box.x, thumb_box.y);
	}

	/* title */
	const char *title = view_get_string_prop(view, "title");
	if (title) {
		item->normal_title = create_title(item->tree, switcher_theme,
			title, theme->osd_label_text_color,
			theme->osd_bg_color, title_y);
		item->active_title = create_title(item->tree, switcher_theme,
			title, theme->osd_label_text_color,
			switcher_theme->item_active_bg_color, title_y);
	}

	/* icon */
	int icon_size = switcher_theme->item_icon_size;
	struct scaled_icon_buffer *icon_buffer = scaled_icon_buffer_create(
		item->tree, server, icon_size, icon_size);
	scaled_icon_buffer_set_view(icon_buffer, view);
	int x = (switcher_theme->item_width - icon_size) / 2;
	int y = title_y - padding - icon_size + 10; /* slide by 10px */
	wlr_scene_node_set_position(&icon_buffer->scene_buffer->node, x, y);

	return item;
}

static void
get_items_geometry(struct output *output, struct theme *theme,
		int nr_thumbs, int *nr_rows, int *nr_cols)
{
	struct window_switcher_thumbnail_theme *thumb_theme =
		&theme->osd_window_switcher_thumbnail;
	int output_width = output->wlr_output->width / output->wlr_output->scale;

	int max_bg_width = thumb_theme->max_width;
	if (thumb_theme->max_width_is_percent) {
		max_bg_width = output_width * thumb_theme->max_width / 100;
	}

	*nr_rows = 1;
	*nr_cols = nr_thumbs;
	while (1) {
		assert(*nr_rows <= nr_thumbs);
		int bg_width = *nr_cols * thumb_theme->item_width
			+ theme->osd_border_width + thumb_theme->padding;
		if (bg_width < max_bg_width) {
			break;
		}
		if (*nr_rows >= nr_thumbs) {
			break;
		}
		(*nr_rows)++;
		*nr_cols = ceilf((float)nr_thumbs / *nr_rows);
	}
}

static void
osd_thumbnail_create(struct output *output, struct wl_array *views)
{
	assert(!output->osd_scene.tree);

	struct theme *theme = output->server->theme;
	struct window_switcher_thumbnail_theme *switcher_theme =
		&theme->osd_window_switcher_thumbnail;
	int padding = theme->osd_border_width + switcher_theme->padding;

	output->osd_scene.tree = wlr_scene_tree_create(output->osd_tree);

	int nr_views = wl_array_len(views);
	assert(nr_views > 0);
	int nr_rows, nr_cols;
	get_items_geometry(output, theme, nr_views, &nr_rows, &nr_cols);

	/* items */
	struct view **view;
	int index = 0;
	wl_array_for_each(view, views) {
		struct osd_thumbnail_scene_item *item = create_item_scene(
			output->osd_scene.tree, *view, output);
		if (!item) {
			break;
		}
		int x = (index % nr_cols) * switcher_theme->item_width + padding;
		int y = (index / nr_cols) * switcher_theme->item_height + padding;
		wlr_scene_node_set_position(&item->tree->node, x, y);
		index++;
	}

	/* background */
	struct lab_scene_rect_options bg_opts = {
		.width = nr_cols * switcher_theme->item_width + 2 * padding,
		.height = nr_rows * switcher_theme->item_height + 2 * padding,
		.bg_color = theme->osd_bg_color,
		.nr_borders = 1,
		.border_width = theme->osd_border_width,
		.border_colors = (float *[1]) { theme->osd_border_color },
	};
	struct lab_scene_rect *bg =
		lab_scene_rect_create(output->osd_scene.tree, &bg_opts);
	wlr_scene_node_lower_to_bottom(&bg->tree->node);

	/* center */
	struct wlr_box usable = output_usable_area_in_layout_coords(output);
	int lx = usable.x + (usable.width - bg_opts.width) / 2;
	int ly = usable.y + (usable.height - bg_opts.height) / 2;
	wlr_scene_node_set_position(&output->osd_scene.tree->node, lx, ly);
}

static void
osd_thumbnail_update(struct output *output)
{
	struct osd_thumbnail_scene_item *item;
	wl_array_for_each(item, &output->osd_scene.items) {
		bool active = (item->view == output->server->osd_state.cycle_view);
		wlr_scene_node_set_enabled(&item->active_bg->tree->node, active);
		wlr_scene_node_set_enabled(
			&item->active_title->scene_buffer->node, active);
		wlr_scene_node_set_enabled(
			&item->normal_title->scene_buffer->node, !active);
	}
}

struct osd_impl osd_thumbnail_impl = {
	.create = osd_thumbnail_create,
	.update = osd_thumbnail_update,
};
