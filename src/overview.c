// SPDX-License-Identifier: GPL-2.0-only
/*
 * Overview layout algorithm based on KWin's expolayout
 * Original: SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 * Original: SPDX-FileCopyrightText: 2024 Yifan Zhu <fanzhuyifan@gmail.com>
 */
#include <assert.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <wlr/render/allocator.h>
#include <wlr/render/swapchain.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include "common/array.h"
#include "common/lab-scene-rect.h"
#include "common/list.h"
#include "common/mem.h"
#include "common/scene-helpers.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "node.h"
#include "output.h"
#include "overview.h"
#include "theme.h"
#include "view.h"

/* Layout parameters (matching KWin defaults) */
#define SEARCH_TOLERANCE 0.2
#define IDEAL_WIDTH_RATIO 0.8
#define RELATIVE_MARGIN 0.07
#define RELATIVE_MIN_LENGTH 0.15
#define MAX_GAP_RATIO 1.5
#define MAX_SCALE 1.0

static struct overview_state overview;

/* Window rect with margins for layout calculation */
struct window_rect {
	double x, y, width, height;
	double center_x, center_y;
	int id;
	struct view *view;
};

/* A layer (row) of windows */
struct layer {
	double max_width;
	double max_height;
	double width;
	int *ids;
	int count;
};

/* Layered packing result */
struct layered_packing {
	double max_width;
	double width;
	double height;
	struct layer *layers;
	int layer_count;
	int *all_ids;  /* Single allocation for all layer ids */
};

/* Final layout result */
struct layout_result {
	double x, y, width, height;
};

static void
render_node(struct wlr_render_pass *pass,
		struct wlr_scene_node *node, int x, int y)
{
	switch (node->type) {
	case WLR_SCENE_NODE_TREE: {
		struct wlr_scene_tree *tree = wlr_scene_tree_from_node(node);
		struct wlr_scene_node *child;
		wl_list_for_each(child, &tree->children, link) {
			render_node(pass, child, x + node->x, y + node->y);
		}
		break;
	}
	case WLR_SCENE_NODE_BUFFER: {
		struct wlr_scene_buffer *scene_buffer =
			wlr_scene_buffer_from_node(node);
		if (!scene_buffer->buffer) {
			break;
		}
		struct wlr_texture *texture = NULL;
		struct wlr_client_buffer *client_buffer =
			wlr_client_buffer_get(scene_buffer->buffer);
		if (client_buffer) {
			texture = client_buffer->texture;
		}
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
		break;
	}
	case WLR_SCENE_NODE_RECT:
		break;
	}
}

static struct wlr_buffer *
render_thumb(struct output *output, struct view *view)
{
	if (!view->content_tree) {
		return NULL;
	}
	struct wlr_buffer *buffer = wlr_allocator_create_buffer(server.allocator,
		view->current.width, view->current.height,
		&output->wlr_output->swapchain->format);
	if (!buffer) {
		wlr_log(WLR_ERROR, "failed to allocate buffer for overview thumbnail");
		return NULL;
	}
	struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(
		server.renderer, buffer, NULL);
	render_node(pass, &view->content_tree->node, 0, 0);
	if (!wlr_render_pass_submit(pass)) {
		wlr_log(WLR_ERROR, "failed to submit render pass");
		wlr_buffer_drop(buffer);
		return NULL;
	}
	return buffer;
}

struct overview_item *
node_overview_item_from_node(struct wlr_scene_node *wlr_scene_node)
{
	assert(wlr_scene_node->data);
	struct node_descriptor *node_descriptor = wlr_scene_node->data;
	assert(node_descriptor->type == LAB_NODE_OVERVIEW_ITEM);
	return (struct overview_item *)node_descriptor->data;
}

static struct overview_item *
create_item_at(struct wlr_scene_tree *parent, struct view *view,
		struct output *output, int x, int y, int width, int height)
{
	struct overview_item *item = znew(*item);
	wl_list_append(&overview.items, &item->link);

	struct wlr_scene_tree *tree = lab_wlr_scene_tree_create(parent);
	node_descriptor_create(&tree->node, LAB_NODE_OVERVIEW_ITEM, view, item);
	item->tree = tree;
	item->view = view;

	wlr_scene_node_set_position(&tree->node, x, y);

	/* Border/highlight for item */
	struct theme *theme = rc.theme;
	struct lab_scene_rect_options opts = {
		.border_colors = (float *[1]) {
			theme->osd_window_switcher_thumbnail.item_active_border_color
		},
		.nr_borders = 1,
		.border_width = 2,
		.bg_color = theme->overview_bg_color,
		.width = width,
		.height = height,
	};
	lab_scene_rect_create(tree, &opts);

	/* Invisible hitbox for mouse clicks */
	lab_wlr_scene_rect_create(tree, width, height, (float[4]) {0});

	/* Thumbnail */
	struct wlr_buffer *thumb_buffer = render_thumb(output, view);
	if (thumb_buffer) {
		struct wlr_scene_buffer *thumb_scene_buffer =
			lab_wlr_scene_buffer_create(tree, thumb_buffer);
		wlr_buffer_drop(thumb_buffer);
		wlr_scene_buffer_set_dest_size(thumb_scene_buffer,
			width, height);
	}

	return item;
}

/* Compare windows by height (ascending), then by y position */
static int
compare_by_height(const void *a, const void *b)
{
	const struct window_rect *wa = a;
	const struct window_rect *wb = b;

	if (wa->height != wb->height) {
		return (wa->height < wb->height) ? -1 : 1;
	}
	if (wa->center_y != wb->center_y) {
		return (wa->center_y < wb->center_y) ? -1 : 1;
	}

	/* Stability: fall back to creation_id */
	if (wa->view->creation_id != wb->view->creation_id) {
		return (wa->view->creation_id < wb->view->creation_id) ? 1 : -1;
	}

	return 0;
}

/* Temporary struct for sorting layer windows by x position */
struct sort_item {
	int id;
	double center_x;
};

/* Compare sort_items by x position */
static int
compare_by_x(const void *a, const void *b)
{
	const struct sort_item *sa = a;
	const struct sort_item *sb = b;
	if (sa->center_x != sb->center_x) {
		return (sa->center_x < sb->center_x) ? -1 : 1;
	}
	return 0;
}

/*
 * Weight function for LWS algorithm
 * Penalizes non-uniform row widths
 */
static double
layer_weight(double ideal_width, double max_width,
		double *cum_widths, int start, int end, int count)
{
	double width = cum_widths[end] - cum_widths[start];

	if (width < ideal_width) {
		double diff = (width - ideal_width) / ideal_width;
		return diff * diff;
	} else {
		double penalty_factor = count;
		double diff = (width - ideal_width) / (max_width - ideal_width);
		return penalty_factor * diff * diff;
	}
}

/*
 * Simplified LWS algorithm to find optimal layer boundaries
 * Returns array of layer start positions
 */
static void
get_layer_start_positions(double max_width, double ideal_width,
		int count, double *cum_widths, int *out_layer_count, int *layer_positions,
		double *least_weight, int *layer_start)
{
	/* least_weight[i] = minimum weight to arrange first i windows */
	/* layer_start[i] = where last layer starts for first i windows */

	least_weight[0] = 0;

	for (int i = 1; i <= count; i++) {
		double best_weight = DBL_MAX;
		int best_start = 0;

		for (int j = 0; j < i; j++) {
			double w = least_weight[j] +
				layer_weight(ideal_width, max_width,
					cum_widths, j, i, count);
			if (w < best_weight) {
				best_weight = w;
				best_start = j;
			}
		}

		least_weight[i] = best_weight;
		layer_start[i] = best_start;
	}

	/* Reconstruct layer boundaries */
	int pos_count = 0;

	int current = count;
	while (current > 0) {
		layer_positions[pos_count++] = current;
		current = layer_start[current];
	}
	layer_positions[pos_count++] = 0;

	/* Reverse in place */
	for (int i = 0; i < pos_count / 2; i++) {
		int tmp = layer_positions[i];
		layer_positions[i] = layer_positions[pos_count - 1 - i];
		layer_positions[pos_count - 1 - i] = tmp;
	}

	*out_layer_count = pos_count;
}

/*
 * Create a layered packing from window sizes
 */
static struct layered_packing
create_packing(double max_width, struct window_rect *sorted_windows,
		int count, int *layer_positions, int layer_count)
{
	struct layered_packing packing = {
		.max_width = max_width,
		.width = 0,
		.height = 0,
		.layers = znew_n(struct layer, layer_count - 1),
		.layer_count = layer_count - 1,
		.all_ids = znew_n(int, count),
	};

	int id_offset = 0;
	for (int l = 0; l < layer_count - 1; l++) {
		int start = layer_positions[l];
		int end = layer_positions[l + 1];
		int layer_window_count = end - start;

		struct layer *layer = &packing.layers[l];
		layer->max_width = max_width;
		layer->max_height = sorted_windows[end - 1].height;
		layer->width = 0;
		layer->ids = packing.all_ids + id_offset;
		layer->count = layer_window_count;
		id_offset += layer_window_count;

		for (int i = start; i < end; i++) {
			layer->ids[i - start] = sorted_windows[i].id;
			layer->width += sorted_windows[i].width;
		}

		if (layer->width > packing.width) {
			packing.width = layer->width;
		}
		packing.height += layer->max_height;
	}

	return packing;
}

static void
evaluate_packing(struct window_rect *sorted_windows,
		int count, int *layer_positions, int layer_count,
		double *out_width, double *out_height)
{
	double max_width = 0.0;
	double total_height = 0.0;

	for (int l = 0; l < layer_count - 1; l++) {
		int start = layer_positions[l];
		int end = layer_positions[l + 1];

		double layer_width = 0.0;

		for (int i = start; i < end; i++) {
			layer_width += sorted_windows[i].width;
		}

		if (layer_width > max_width) {
			max_width = layer_width;
		}
		total_height += sorted_windows[end - 1].height;
	}

	*out_width = max_width;
	*out_height = total_height;
}

static void
free_packing(struct layered_packing *packing)
{
	free(packing->all_ids);
	free(packing->layers);
}

/*
 * Find good packing using binary search on strip width
 */
static struct layered_packing
find_good_packing(double area_width, double area_height,
		struct window_rect *windows, int count)
{
	/*
	 * Allocate all temporary arrays in a single malloc.
	 * Layout (aligned for double first, then int):
	 *   sorted:             count * sizeof(window_rect)
	 *   cum_widths:         (count+1) * sizeof(double)
	 *   tmp_least_weight:   (count+1) * sizeof(double)
	 *   min_layer_positions:(count+2) * sizeof(int)
	 *   max_layer_positions:(count+2) * sizeof(int)
	 *   mid_layer_positions:(count+2) * sizeof(int)
	 *   tmp_layer_start:    (count+1) * sizeof(int)
	 */
	size_t sorted_size = count * sizeof(struct window_rect);
	size_t cum_widths_size = (count + 1) * sizeof(double);
	size_t least_weight_size = (count + 1) * sizeof(double);
	size_t layer_pos_size = (count + 2) * sizeof(int);
	size_t layer_start_size = (count + 1) * sizeof(int);

	size_t total_size = sorted_size + cum_widths_size + least_weight_size
		+ 3 * layer_pos_size + layer_start_size;

	char *buf = xmalloc(total_size);
	char *ptr = buf;
	struct window_rect *sorted = (struct window_rect *)ptr;
	ptr += sorted_size;
	double *cum_widths = (double *)ptr;
	ptr += cum_widths_size;
	double *tmp_least_weight = (double *)ptr;
	ptr += least_weight_size;
	int *min_layer_positions = (int *)ptr;
	ptr += layer_pos_size;
	int *max_layer_positions = (int *)ptr;
	ptr += layer_pos_size;
	int *mid_layer_positions = (int *)ptr;
	ptr += layer_pos_size;
	int *tmp_layer_start = (int *)ptr;

	/* Sort windows by height */
	memcpy(sorted, windows, count * sizeof(struct window_rect));
	qsort(sorted, count, sizeof(struct window_rect), compare_by_height);

	/* Calculate cumulative widths */
	double strip_width_min = 0;
	double strip_width_max = 0;

	cum_widths[0] = 0;
	for (int i = 0; i < count; i++) {
		cum_widths[i + 1] = cum_widths[i] + sorted[i].width;
		if (sorted[i].width > strip_width_min) {
			strip_width_min = sorted[i].width;
		}
		strip_width_max += sorted[i].width;
	}

	strip_width_min /= IDEAL_WIDTH_RATIO;
	strip_width_max /= IDEAL_WIDTH_RATIO;

	double target_ratio = area_height / area_width;

	/* Binary search for optimal strip width */
	struct layered_packing best_packing = {0};

	int min_layer_count;
	double min_width;
	double min_height;
	int max_layer_count;
	double max_width;
	double max_height;
	int mid_layer_count;
	double mid_width;
	double mid_height;

	int *best_layer_positions;
	int best_layer_count;
	double best_strip_width;

	/* Try minimum width */
	get_layer_start_positions(strip_width_min,
		strip_width_min * IDEAL_WIDTH_RATIO, count, cum_widths,
		&min_layer_count, min_layer_positions,
		tmp_least_weight, tmp_layer_start);
	evaluate_packing(sorted, count, min_layer_positions, min_layer_count,
			&min_width, &min_height);

	double ratio_high = min_height / min_width;
	if (ratio_high <= target_ratio) {
		best_layer_positions = min_layer_positions;
		best_layer_count = min_layer_count;
		best_strip_width = strip_width_min;
		goto result;
	}

	/* Try maximum width */
	get_layer_start_positions(strip_width_max,
		strip_width_max * IDEAL_WIDTH_RATIO, count, cum_widths,
		&max_layer_count, max_layer_positions,
		tmp_least_weight, tmp_layer_start);
	evaluate_packing(sorted, count, max_layer_positions, max_layer_count,
			&max_width, &max_height);

	double ratio_low = max_height / max_width;
	if (ratio_low >= target_ratio) {
		best_layer_positions = max_layer_positions;
		best_layer_count = max_layer_count;
		best_strip_width = strip_width_max;
		goto result;
	}

	/* Binary search (max 10 iterations to guarantee termination) */
	for (int iter = 0; iter < 10 &&
			strip_width_max / strip_width_min > 1 + SEARCH_TOLERANCE; iter++) {
		double strip_width_mid = sqrt(strip_width_min * strip_width_max);

		get_layer_start_positions(strip_width_mid,
			strip_width_mid * IDEAL_WIDTH_RATIO, count, cum_widths,
			&mid_layer_count, mid_layer_positions,
			tmp_least_weight, tmp_layer_start);

		evaluate_packing(sorted, count, mid_layer_positions, mid_layer_count,
				&mid_width, &mid_height);

		double ratio_mid = mid_height / mid_width;

		if (ratio_mid > target_ratio) {
			strip_width_min = strip_width_mid;
			// swap
			int *temp = min_layer_positions;
			min_layer_positions = mid_layer_positions;
			mid_layer_positions = temp;

			min_layer_count = mid_layer_count;
			min_width = mid_width;
			min_height = mid_height;
			ratio_high = ratio_mid;
		} else {
			strip_width_max = strip_width_mid;
			// swap
			int *temp = max_layer_positions;
			max_layer_positions = mid_layer_positions;
			mid_layer_positions = temp;

			max_layer_count = mid_layer_count;
			max_width = mid_width;
			max_height = mid_height;
			ratio_low = ratio_mid;
		}
	}

	/* Choose packing with better scale */
	double scale_min = fmin(area_width / min_width,
		area_height / min_height);
	double scale_max = fmin(area_width / max_width,
		area_height / max_height);

	if (scale_min > scale_max) {
		best_layer_positions = min_layer_positions;
		best_layer_count = min_layer_count;
		best_strip_width = strip_width_min;
	} else {
		best_layer_positions = max_layer_positions;
		best_layer_count = max_layer_count;
		best_strip_width = strip_width_max;
	}

result:
	best_packing = create_packing(best_strip_width,
		sorted, count, best_layer_positions, best_layer_count);
	free(buf);

	return best_packing;
}

/*
 * Apply packing and compute final window positions
 */
static void
apply_packing(double area_x, double area_y, double area_width, double area_height,
		double margin, struct layered_packing *packing,
		struct window_rect *windows, int count,
		struct layout_result *results)
{
	/* Scale packing to fit area */
	double scale = fmin(area_width / packing->width,
		area_height / packing->height);
	scale = fmin(scale, MAX_SCALE);

	double scaled_margin = margin * scale;

	/* Maximum additional gap */
	double max_gap_y = MAX_GAP_RATIO * 2 * scaled_margin;
	double max_gap_x = MAX_GAP_RATIO * 2 * scaled_margin;

	/* Center align vertically */
	double extra_y = area_height - packing->height * scale;
	double gap_y = fmin(max_gap_y, extra_y / (packing->layer_count + 1));
	double y = area_y + (extra_y - gap_y * (packing->layer_count - 1)) / 2;

	/* Find max layer count for sort_items allocation */
	int max_layer_window_count = 0;
	for (int l = 0; l < packing->layer_count; l++) {
		if (packing->layers[l].count > max_layer_window_count) {
			max_layer_window_count = packing->layers[l].count;
		}
	}
	struct sort_item *sort_items = xmalloc(
		max_layer_window_count * sizeof(*sort_items));

	for (int l = 0; l < packing->layer_count; l++) {
		struct layer *layer = &packing->layers[l];

		/* Sort windows within layer by x position */
		for (int i = 0; i < layer->count; i++) {
			int id = layer->ids[i];
			sort_items[i].id = id;
			sort_items[i].center_x = windows[id].center_x;
		}
		qsort(sort_items, layer->count, sizeof(*sort_items), compare_by_x);
		for (int i = 0; i < layer->count; i++) {
			layer->ids[i] = sort_items[i].id;
		}

		double extra_x = area_width - layer->width * scale;
		double gap_x = fmin(max_gap_x, extra_x / (layer->count + 1));
		double x = area_x + (extra_x - gap_x * (layer->count - 1)) / 2;

		for (int i = 0; i < layer->count; i++) {
			int id = layer->ids[i];
			struct window_rect *win = &windows[id];

			/* Center align vertically within layer */
			double new_y = y + (layer->max_height - win->height) * scale / 2;

			/* Apply scaling and margins */
			results[id].x = x + scaled_margin;
			results[id].y = new_y + scaled_margin;
			results[id].width = (win->width - 2 * margin) * scale;
			results[id].height = (win->height - 2 * margin) * scale;

			x += win->width * scale + gap_x;
		}

		y += layer->max_height * scale + gap_y;
	}

	free(sort_items);
}

void
overview_begin(void)
{
	if (server.input_mode != LAB_INPUT_STATE_PASSTHROUGH) {
		return;
	}

	struct output *output = output_nearest_to_cursor();
	if (!output || !output_is_usable(output)) {
		return;
	}

	/* Get output geometry */
	struct wlr_box output_box;
	wlr_output_layout_get_box(server.output_layout, output->wlr_output,
		&output_box);

	/* Collect all views */
	struct wl_array views;
	wl_array_init(&views);
	view_array_append(&views,
		LAB_VIEW_CRITERIA_CURRENT_WORKSPACE
		| LAB_VIEW_CRITERIA_NO_SKIP_WINDOW_SWITCHER);

	int count = wl_array_len(&views);
	if (count == 0) {
		wl_array_release(&views);
		return;
	}

	/* Calculate margins based on short side */
	double short_side = fmin(output_box.width, output_box.height);
	double margin = RELATIVE_MARGIN * short_side;
	double min_length = RELATIVE_MIN_LENGTH * short_side;

	/* Available area for layout */
	double area_width = output_box.width - 2 * margin;
	double area_height = output_box.height - 2 * margin;

	/* Create window rects with margins */
	struct window_rect *windows = znew_n(*windows, count);
	struct view **view_ptr;
	int idx = 0;
	wl_array_for_each(view_ptr, &views) {
		struct view *v = *view_ptr;
		double w = fmax(v->current.width, min_length) + 2 * margin;
		double h = fmax(v->current.height, min_length) + 2 * margin;

		/* Clip oversized windows */
		w = fmin(w, 4 * area_width);
		h = fmin(h, 4 * area_height);

		windows[idx].x = v->current.x;
		windows[idx].y = v->current.y;
		windows[idx].width = w;
		windows[idx].height = h;
		windows[idx].center_x = v->current.x + v->current.width / 2.0;
		windows[idx].center_y = v->current.y + v->current.height / 2.0;
		windows[idx].id = idx;
		windows[idx].view = v;
		idx++;
	}

	/* Find good packing */
	struct layered_packing packing = find_good_packing(
		area_width, area_height, windows, count);

	/* Apply packing to get final positions */
	struct layout_result *results = znew_n(*results, count);
	apply_packing(0, 0, area_width, area_height, margin, &packing,
		windows, count, results);

	/* Initialize overview state */
	overview.active = true;
	overview.output = output;
	wl_list_init(&overview.items);

	/* Create overview tree */
	overview.tree = lab_wlr_scene_tree_create(&server.scene->tree);
	wlr_scene_node_raise_to_top(&overview.tree->node);

	/* Background overlay */
	struct theme *theme = rc.theme;
	float bg_color[4] = {
		theme->overview_bg_color[0],
		theme->overview_bg_color[1],
		theme->overview_bg_color[2],
		theme->overview_bg_color[3],
	};
	struct wlr_scene_rect *bg = lab_wlr_scene_rect_create(overview.tree,
		output_box.width, output_box.height, bg_color);
	wlr_scene_node_set_position(&bg->node, output_box.x, output_box.y);

	/* Create content tree */
	struct wlr_scene_tree *content_tree =
		lab_wlr_scene_tree_create(overview.tree);
	wlr_scene_node_set_position(&content_tree->node,
		output_box.x + (int)margin,
		output_box.y + (int)margin);

	/* Create items at calculated positions */
	for (int i = 0; i < count; i++) {
		create_item_at(content_tree, windows[i].view, output,
			(int)results[i].x, (int)results[i].y,
			(int)results[i].width, (int)results[i].height);
	}

	free_packing(&packing);
	free(results);
	free(windows);
	wl_array_release(&views);

	seat_focus_override_begin(&server.seat,
		LAB_INPUT_STATE_OVERVIEW, LAB_CURSOR_DEFAULT);

	cursor_update_focus();
}

void
overview_finish(bool focus_selected)
{
	if (!overview.active) {
		return;
	}

	if (overview.tree) {
		wlr_scene_node_destroy(&overview.tree->node);
	}

	struct overview_item *item, *tmp;
	wl_list_for_each_safe(item, tmp, &overview.items, link) {
		wl_list_remove(&item->link);
		free(item);
	}

	overview = (struct overview_state){0};
	wl_list_init(&overview.items);

	seat_focus_override_end(&server.seat, /*restore_focus*/ !focus_selected);
	cursor_update_focus();
}

void
overview_on_cursor_release(struct wlr_scene_node *node)
{
	assert(server.input_mode == LAB_INPUT_STATE_OVERVIEW);

	struct overview_item *item = node_overview_item_from_node(node);
	struct view *view = item->view;

	overview_finish(/*focus_selected*/ true);

	if (view) {
		desktop_focus_view(view, /*raise*/ true);
	}
}

void
overview_toggle(void)
{
	if (overview.active) {
		overview_finish(/*focus_selected*/ false);
	} else {
		overview_begin();
	}
}
