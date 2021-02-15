/*
 * output.c: labwc output and rendering
 *
 * Copyright (C) 2019-2021 Johan Malm
 * Copyright (C) 2020 The Sway authors
 */

#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/util/region.h>
#include "labwc.h"
#include "menu/menu.h"
#include "theme/theme.h"
#include "layers.h"

//#define DEBUG 1

typedef void (*surface_iterator_func_t)(struct output *output,
		struct wlr_surface *surface, struct wlr_box *box,
		void *user_data);

struct surface_iterator_data {
	surface_iterator_func_t user_iterator;
	void *user_data;
	struct output *output;
	double ox, oy;
};

static bool
intersects_with_output(struct output *output,
		struct wlr_output_layout *output_layout,
		struct wlr_box *surface_box)
{
	/* The resolution can change if outputs are rotated */
	struct wlr_box output_box = {0};
	wlr_output_effective_resolution(output->wlr_output, &output_box.width,
		&output_box.height);
	struct wlr_box intersection;
	return wlr_box_intersection(&intersection, &output_box, surface_box);
}

static void
output_for_each_surface_iterator(struct wlr_surface *surface, int sx, int sy,
		void *user_data)
{
	struct surface_iterator_data *data = user_data;
	struct output *output = data->output;
	if (!surface || !wlr_surface_has_buffer(surface)) {
		return;
	}
	struct wlr_box surface_box = {
		.x = data->ox + sx + surface->sx,
		.y = data->oy + sy + surface->sy,
		.width = surface->current.width,
		.height = surface->current.height,
	};

	if (!intersects_with_output(output, output->server->output_layout,
				    &surface_box)) {
		return;
	}
	data->user_iterator(data->output, surface, &surface_box,
		data->user_data);
}

void
output_surface_for_each_surface(struct output *output,
		struct wlr_surface *surface, double ox, double oy,
		surface_iterator_func_t iterator, void *user_data)
{
	struct surface_iterator_data data = {
		.user_iterator = iterator,
		.user_data = user_data,
		.output = output,
		.ox = ox,
		.oy = oy,
	};
	if (!surface) {
		return;
	}
	wlr_surface_for_each_surface(surface,
		output_for_each_surface_iterator, &data);
}

struct render_data {
	pixman_region32_t *damage;
};

int
scale_length(int length, int offset, float scale)
{
	return round((offset + length) * scale) - round(offset * scale);
}

void
scale_box(struct wlr_box *box, float scale)
{
	box->width = scale_length(box->width, box->x, scale);
	box->height = scale_length(box->height, box->y, scale);
	box->x = round(box->x * scale);
	box->y = round(box->y * scale);
}

static void
scissor_output(struct wlr_output *output, pixman_box32_t *rect)
{
	struct wlr_renderer *renderer = wlr_backend_get_renderer(output->backend);

	struct wlr_box box = {
		.x = rect->x1,
		.y = rect->y1,
		.width = rect->x2 - rect->x1,
		.height = rect->y2 - rect->y1,
	};

	int output_width, output_height;
	wlr_output_transformed_resolution(output, &output_width, &output_height);
	enum wl_output_transform transform =
		wlr_output_transform_invert(output->transform);
	wlr_box_transform(&box, &box, transform, output_width, output_height);

	wlr_renderer_scissor(renderer, &box);
}

static void
render_texture(struct wlr_output *wlr_output, pixman_region32_t *output_damage,
		struct wlr_texture *texture, const struct wlr_box *box,
		const float matrix[static 9])
{
	struct wlr_renderer *renderer =
		wlr_backend_get_renderer(wlr_output->backend);

	pixman_region32_t damage;
	pixman_region32_init(&damage);
	pixman_region32_union_rect(&damage, &damage, box->x, box->y,
		box->width, box->height);
	pixman_region32_intersect(&damage, &damage, output_damage);
	if (!pixman_region32_not_empty(&damage)) {
		goto damage_finish;
	}

	int nrects;
	pixman_box32_t *rects = pixman_region32_rectangles(&damage, &nrects);
	for (int i = 0; i < nrects; i++) {
		scissor_output(wlr_output, &rects[i]);
		wlr_render_texture_with_matrix(renderer, texture, matrix, 1.0f);
	}

damage_finish:
	pixman_region32_fini(&damage);
}

static void
render_surface_iterator(struct output *output, struct wlr_surface *surface,
		struct wlr_box *box, void *user_data)
{
	struct render_data *data = user_data;
	struct wlr_output *wlr_output = output->wlr_output;
	pixman_region32_t *output_damage = data->damage;

	struct wlr_texture *texture = wlr_surface_get_texture(surface);
	if (!texture) {
		wlr_log(WLR_DEBUG, "Cannot obtain surface texture");
		return;
	}

	scale_box(box, wlr_output->scale);

	float matrix[9];
	enum wl_output_transform transform =
		wlr_output_transform_invert(surface->current.transform);
	wlr_matrix_project_box(matrix, box, transform, 0.0f,
		wlr_output->transform_matrix);

	render_texture(wlr_output, output_damage, texture, box, matrix);
}

#if HAVE_XWAYLAND
void
output_unmanaged_for_each_surface(struct output *output,
		struct wl_list *unmanaged, surface_iterator_func_t iterator,
		void *user_data)
{
	struct xwayland_unmanaged *unmanaged_surface;
	wl_list_for_each(unmanaged_surface, unmanaged, link) {
		struct wlr_xwayland_surface *xsurface =
			unmanaged_surface->xwayland_surface;
		double ox = unmanaged_surface->lx, oy = unmanaged_surface->ly;
		wlr_output_layout_output_coords(
			output->server->output_layout, output->wlr_output, &ox, &oy);
		output_surface_for_each_surface(output, xsurface->surface, ox, oy,
			iterator, user_data);
	}
}

static void render_unmanaged(struct output *output, pixman_region32_t *damage,
		struct wl_list *unmanaged) {
	struct render_data data = {
		.damage = damage,
	};
	output_unmanaged_for_each_surface(output, unmanaged,
		render_surface_iterator, &data);
}
#endif

static void
output_view_for_each_surface(struct output *output, struct view *view,
		surface_iterator_func_t iterator, void *user_data)
{
	struct surface_iterator_data data = {
		.user_iterator = iterator,
		.user_data = user_data,
		.output = output,
		.ox = view->x,
		.oy = view->y,
	};

	wlr_output_layout_output_coords(output->server->output_layout,
		output->wlr_output, &data.ox, &data.oy);
	view_for_each_surface(view, output_for_each_surface_iterator, &data);
}

void
output_view_for_each_popup(struct output *output, struct view *view,
		surface_iterator_func_t iterator, void *user_data)
{
	struct surface_iterator_data data = {
		.user_iterator = iterator,
		.user_data = user_data,
		.output = output,
		.ox = view->x,
		.oy = view->y,
	};

	wlr_output_layout_output_coords(output->server->output_layout,
		output->wlr_output, &data.ox, &data.oy);
	view_for_each_popup(view, output_for_each_surface_iterator, &data);
}

/* for sending frame done */
void output_layer_for_each_surface(struct output *output,
		struct wl_list *layer_surfaces, surface_iterator_func_t iterator,
		void *user_data) {
	struct lab_layer_surface *layer_surface;
	wl_list_for_each(layer_surface, layer_surfaces, link) {
		struct wlr_layer_surface_v1 *wlr_layer_surface_v1 =
			layer_surface->layer_surface;
		output_surface_for_each_surface(output, wlr_layer_surface_v1->surface,
			layer_surface->geo.x, layer_surface->geo.y, iterator,
			user_data);
		/* TODO: handle popups */
	}
}

static void
output_for_each_surface(struct output *output, surface_iterator_func_t iterator,
		void *user_data)
{
	output_layer_for_each_surface(output,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND],
		iterator, user_data);
	output_layer_for_each_surface(output,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM],
		iterator, user_data);

	struct view *view;
	wl_list_for_each_reverse(view, &output->server->views, link) {
		if (!view->mapped) {
			continue;
		}
		output_view_for_each_surface(output, view, iterator, user_data);
	}

#if HAVE_XWAYLAND
	output_unmanaged_for_each_surface(output, &output->server->unmanaged_surfaces,
		iterator, user_data);
#endif

	output_layer_for_each_surface(output,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP],
		iterator, user_data);
	output_layer_for_each_surface(output,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY],
		iterator, user_data);
}

struct send_frame_done_data {
	struct timespec when;
};

static void
send_frame_done_iterator(struct output *output, struct wlr_surface *surface,
		struct wlr_box *box, void *user_data)
{
	struct send_frame_done_data *data = user_data;
	wlr_surface_send_frame_done(surface, &data->when);
}

static void
send_frame_done(struct output *output, struct send_frame_done_data *data)
{
	output_for_each_surface(output, send_frame_done_iterator, data);
}

void
render_rect(struct output *output, pixman_region32_t *output_damage,
		const struct wlr_box *_box, float color[static 4])
{
	struct wlr_output *wlr_output = output->wlr_output;
	struct wlr_renderer *renderer =
		wlr_backend_get_renderer(wlr_output->backend);

	struct wlr_box box;
	memcpy(&box, _box, sizeof(struct wlr_box));

	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(output->server->output_layout,
		output->wlr_output, &ox, &oy);
	box.x += ox;
	box.y += oy;

	pixman_region32_t damage;
	pixman_region32_init(&damage);
	pixman_region32_union_rect(&damage, &damage, box.x, box.y,
		box.width, box.height);
	pixman_region32_intersect(&damage, &damage, output_damage);
	bool damaged = pixman_region32_not_empty(&damage);
	if (!damaged) {
		goto damage_finish;
	}

	int nrects;
	pixman_box32_t *rects = pixman_region32_rectangles(&damage, &nrects);
	for (int i = 0; i < nrects; ++i) {
		scissor_output(wlr_output, &rects[i]);
		wlr_render_rect(renderer, &box, color,
			wlr_output->transform_matrix);
	}

damage_finish:
	pixman_region32_fini(&damage);
}

void
render_rect_unfilled(struct output *output, pixman_region32_t *output_damage,
		const struct wlr_box *_box, float color[static 4])
{
	struct wlr_box box;
	memcpy(&box, _box, sizeof(struct wlr_box));
	box.height = 1;
	render_rect(output, output_damage, &box, color);
	box.y += _box->height - 1;
	render_rect(output, output_damage, &box, color);
	memcpy(&box, _box, sizeof(struct wlr_box));
	box.width = 1;
	render_rect(output, output_damage, &box, color);
	box.x += _box->width - 1;
	render_rect(output, output_damage, &box, color);
}

static void
shrink(struct wlr_box *box, int size)
{
	box->x += size;
	box->y += size;
	box->width -= 2 * size;
	box->height -= 2 * size;
}

static void
render_cycle_box(struct output *output, pixman_region32_t *output_damage,
		struct view *view)
{
	struct wlr_output_layout *layout = output->server->output_layout;
	double ox = 0, oy = 0;
	struct wlr_box box;
	wlr_output_layout_output_coords(layout, output->wlr_output, &ox, &oy);
	box.x = view->x - view->margin.left + ox;
	box.y = view->y - view->margin.top + oy;
	box.width = view->w + view->margin.left + view->margin.right;
	box.height = view->h + view->margin.top + view->margin.bottom;

	float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	render_rect_unfilled(output, output_damage, &box, white);

	for (int i = 0; i < 4; i++) {
		shrink(&box, 1);
		render_rect_unfilled(output, output_damage, &box, black);
	}
	shrink(&box, 1);
	render_rect_unfilled(output, output_damage, &box, white);
}

static void
render_icon(struct output *output, pixman_region32_t *output_damage,
		struct wlr_box *box, struct wlr_texture *texture)
{
	/* centre-align icon if smaller than designated box */
	struct wlr_box button;
	wlr_texture_get_size(texture, &button.width, &button.height);
	if (box->width > button.width) {
		button.x = box->x + (box->width - button.width) / 2;
	} else {
		button.x = box->x;
		button.width = box->width;
	}
	if (box->height > button.height) {
		button.y = box->y + (box->height - button.height) / 2;
	} else {
		button.y = box->y;
		button.height = box->height;
	}

	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(output->server->output_layout,
		output->wlr_output, &ox, &oy);
	button.x += ox;
	button.y += oy;

	float matrix[9];
	wlr_matrix_project_box(matrix, &button, WL_OUTPUT_TRANSFORM_NORMAL, 0,
		output->wlr_output->transform_matrix);
	render_texture(output->wlr_output, output_damage, texture, &button,
		matrix);
}

static bool
isbutton(enum deco_part deco_part)
{
	return deco_part == LAB_DECO_BUTTON_CLOSE ||
	       deco_part == LAB_DECO_BUTTON_MAXIMIZE ||
	       deco_part == LAB_DECO_BUTTON_ICONIFY;
}

static void
render_deco(struct view *view, struct output *output,
		pixman_region32_t *output_damage)
{
	if (!view->server_side_deco) {
		return;
	}

	/* render border */
	float *color = theme.window_active_handle_bg_color;
	enum deco_part border[4] = {
		LAB_DECO_PART_TOP,
		LAB_DECO_PART_RIGHT,
		LAB_DECO_PART_BOTTOM,
		LAB_DECO_PART_LEFT,
	};
	for (int i = 0; i < 4; i++) {
		struct wlr_box box = deco_box(view, border[i]);
		render_rect(output, output_damage, &box, color);
	}

	/* render title */
	struct wlr_seat *seat = view->server->seat.seat;
	if (view->surface == seat->keyboard_state.focused_surface) {
		color = theme.window_active_title_bg_color;
	} else {
		color = theme.window_inactive_title_bg_color;
	}
	struct wlr_box box = deco_box(view, LAB_DECO_PART_TITLE);
	render_rect(output, output_damage, &box, color);

	/* button background */
	struct wlr_cursor *cur = view->server->seat.cursor;
	enum deco_part deco_part = deco_at(view, cur->x, cur->y);
	box = deco_box(view, deco_part);
	if (isbutton(deco_part) &&
			wlr_box_contains_point(&box, cur->x, cur->y)) {
		color = (float[4]){ 0.5, 0.5, 0.5, 0.5 };
		render_rect(output, output_damage, &box, color);
	}

	/* buttons */
	if (view->surface == seat->keyboard_state.focused_surface) {
		box = deco_box(view, LAB_DECO_BUTTON_CLOSE);
		render_icon(output, output_damage, &box,
			theme.xbm_close_active_unpressed);
		box = deco_box(view, LAB_DECO_BUTTON_MAXIMIZE);
		render_icon(output, output_damage, &box,
			theme.xbm_maximize_active_unpressed);
		box = deco_box(view, LAB_DECO_BUTTON_ICONIFY);
		render_icon(output, output_damage, &box,
			theme.xbm_iconify_active_unpressed);
	} else {
		box = deco_box(view, LAB_DECO_BUTTON_CLOSE);
		render_icon(output, output_damage, &box,
			theme.xbm_close_inactive_unpressed);
		box = deco_box(view, LAB_DECO_BUTTON_MAXIMIZE);
		render_icon(output, output_damage, &box,
			theme.xbm_maximize_inactive_unpressed);
		box = deco_box(view, LAB_DECO_BUTTON_ICONIFY);
		render_icon(output, output_damage, &box,
			theme.xbm_iconify_inactive_unpressed);
	}
}

static void
render_rootmenu(struct output *output, pixman_region32_t *output_damage)
{
	struct server *server = output->server;
	float matrix[9];

	struct wlr_output_layout *output_layout = server->output_layout;
	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(output_layout, output->wlr_output,
		&ox, &oy);
	struct menuitem *menuitem;
	wl_list_for_each (menuitem, &server->rootmenu->menuitems, link) {
		struct wlr_texture *t;
		t = menuitem->selected ? menuitem->active_texture :
			menuitem->inactive_texture;
		struct wlr_box box = {
			.x = menuitem->geo_box.x + ox,
			.y = menuitem->geo_box.y + oy,
			.width = menuitem->geo_box.width,
			.height = menuitem->geo_box.height,
		};
		wlr_matrix_project_box(matrix, &box, WL_OUTPUT_TRANSFORM_NORMAL,
			0, output->wlr_output->transform_matrix);
		render_texture(output->wlr_output, output_damage, t,
			&box, matrix);
	}
}

void output_layer_for_each_surface_toplevel(struct output *output,
		struct wl_list *layer_surfaces, surface_iterator_func_t iterator,
		void *user_data)
{
	struct lab_layer_surface *layer_surface;
	wl_list_for_each(layer_surface, layer_surfaces, link) {
		struct wlr_layer_surface_v1 *wlr_layer_surface_v1 =
			layer_surface->layer_surface;
		output_surface_for_each_surface(output,
			wlr_layer_surface_v1->surface, layer_surface->geo.x,
			layer_surface->geo.y, iterator, user_data);
	}
}

static void render_layer_toplevel(struct output *output,
		pixman_region32_t *damage, struct wl_list *layer_surfaces) {
	struct render_data data = {
		.damage = damage,
	};
	output_layer_for_each_surface_toplevel(output, layer_surfaces,
		render_surface_iterator, &data);
}

static void
render_view_toplevels(struct view *view, struct output *output,
		pixman_region32_t *damage)
{
	struct render_data data = {
		.damage = damage,
	};
	double ox = view->x;
	double oy = view->y;
	wlr_output_layout_output_coords(output->server->output_layout,
		output->wlr_output, &ox, &oy);
	output_surface_for_each_surface(output, view->surface, ox, oy,
		render_surface_iterator, &data);
}

static void
render_popup_iterator(struct output *output, struct wlr_surface *surface,
		struct wlr_box *box, void *data)
{
	/* Render this popup's surface */
	render_surface_iterator(output, surface, box, data);

	/* Render this popup's child toplevels */
	output_surface_for_each_surface(output, surface, box->x, box->y,
		render_surface_iterator, data);
}

static void
render_view_popups(struct view *view, struct output *output,
		pixman_region32_t *damage)
{
	struct render_data data = {
		.damage = damage,
	};
	output_view_for_each_popup(output, view, render_popup_iterator, &data);
}

void
output_render(struct output *output, pixman_region32_t *damage)
{
	struct server *server = output->server;
	struct wlr_output *wlr_output = output->wlr_output;

	struct wlr_renderer *renderer =
		wlr_backend_get_renderer(wlr_output->backend);
	if (!renderer) {
		wlr_log(WLR_DEBUG, "no renderer");
		return;
	}

	/* Calls glViewport and some other GL sanity checks */
	wlr_renderer_begin(renderer, wlr_output->width, wlr_output->height);

	if (!pixman_region32_not_empty(damage)) {
		goto renderer_end;
	}

#ifdef DEBUG
	wlr_renderer_clear(renderer, (float[]){0.2f, 0.0f, 0.0f, 1.0f});
#endif

	float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	int nrects;
	pixman_box32_t *rects = pixman_region32_rectangles(damage, &nrects);
	for (int i = 0; i < nrects; i++) {
		scissor_output(wlr_output, &rects[i]);
		wlr_renderer_clear(renderer, color);
	}

	render_layer_toplevel(output, damage,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
	render_layer_toplevel(output, damage,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);

	struct view *view;
	wl_list_for_each_reverse (view, &server->views, link) {
		if (!view->mapped) {
			continue;
		}
		render_deco(view, output, damage);
		render_view_toplevels(view, output, damage);
		render_view_popups(view, output, damage);
	}

#if HAVE_XWAYLAND
	render_unmanaged(output, damage, &output->server->unmanaged_surfaces);
#endif

	/* 'alt-tab' border */
	if (output->server->cycle_view) {
		render_cycle_box(output, damage, output->server->cycle_view);
	}

	render_layer_toplevel(output, damage,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);
	render_layer_toplevel(output, damage,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);

	if (output->server->input_mode == LAB_INPUT_STATE_MENU) {
		render_rootmenu(output, damage);
	}

renderer_end:
	/* Just in case hardware cursors not supported by GPU */
	wlr_output_render_software_cursors(wlr_output, damage);
	wlr_renderer_scissor(renderer, NULL);
	wlr_renderer_end(renderer);

	int output_width, output_height;
	wlr_output_transformed_resolution(wlr_output, &output_width,
		&output_height);

	pixman_region32_t frame_damage;
	pixman_region32_init(&frame_damage);

	enum wl_output_transform transform =
		wlr_output_transform_invert(wlr_output->transform);
	wlr_region_transform(&frame_damage, &output->damage->current,
		transform, output_width, output_height);

#ifdef DEBUG
	pixman_region32_union_rect(&frame_damage, &frame_damage, 0, 0,
		output_width, output_height);
#endif

	wlr_output_set_damage(wlr_output, &frame_damage);
	pixman_region32_fini(&frame_damage);

	if (!wlr_output_commit(wlr_output)) {
		wlr_log(WLR_ERROR, "could not commit output");
	}
}

static void
damage_surface_iterator(struct output *output, struct wlr_surface *surface,
		struct wlr_box *box, void *user_data)
{
	struct wlr_output *wlr_output = output->wlr_output;
	bool whole = *(bool *) user_data;

	scale_box(box, output->wlr_output->scale);

	if (whole) {
		wlr_output_damage_add_box(output->damage, box);
	} else if (pixman_region32_not_empty(&surface->buffer_damage)) {
		pixman_region32_t damage;
		pixman_region32_init(&damage);
		wlr_surface_get_effective_damage(surface, &damage);

		wlr_region_scale(&damage, &damage, wlr_output->scale);
		if (ceil(wlr_output->scale) > surface->current.scale) {
			wlr_region_expand(&damage, &damage,
				ceil(wlr_output->scale) - surface->current.scale);
		}
		pixman_region32_translate(&damage, box->x, box->y);
		wlr_output_damage_add(output->damage, &damage);
		pixman_region32_fini(&damage);
	}
}

void
output_damage_surface(struct output *output, struct wlr_surface *surface,
		double lx, double ly, bool whole)
{
	if (!output->wlr_output->enabled) {
		return;
	}

	double ox = lx, oy = ly;
	wlr_output_layout_output_coords(output->server->output_layout,
		output->wlr_output, &ox, &oy);
	output_surface_for_each_surface(output, surface, ox, oy,
		damage_surface_iterator, &whole);
}

static void
output_damage_frame_notify(struct wl_listener *listener, void *data)
{
	struct output *output = wl_container_of(listener, output, damage_frame);

	if (!output->wlr_output->enabled) {
		return;
	}

	bool needs_frame;
	pixman_region32_t damage;
	pixman_region32_init(&damage);
	if (!wlr_output_damage_attach_render(output->damage,
			&needs_frame, &damage)) {
		return;
	}

	if (needs_frame) {
		output_render(output, &damage);
	} else {
		wlr_output_rollback(output->wlr_output);
	}
	pixman_region32_fini(&damage);

	struct send_frame_done_data frame_data = {0};
	clock_gettime(CLOCK_MONOTONIC, &frame_data.when);
	send_frame_done(output, &frame_data);
}

static void
output_damage_destroy_notify(struct wl_listener *listener, void *data)
{
	struct output *output = wl_container_of(listener, output, damage_destroy);
        wl_list_remove(&output->damage_frame.link);
        wl_list_remove(&output->damage_destroy.link);
}

static void
output_destroy_notify(struct wl_listener *listener, void *data)
{
        struct output *output = wl_container_of(listener, output, destroy);
        wl_list_remove(&output->link);
        wl_list_remove(&output->destroy.link);
}

static void
new_output_notify(struct wl_listener *listener, void *data)
{
	/* This event is rasied by the backend when a new output (aka a display
	 * or monitor) becomes available. */
	struct server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	/*
	 * The mode is a tuple of (width, height, refresh rate).
	 * TODO: support user configuration
	 */
	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	if (mode) {
		wlr_output_set_mode(wlr_output, mode);
		wlr_output_commit(wlr_output);
	}

	struct output *output = calloc(1, sizeof(struct output));
	output->wlr_output = wlr_output;
	output->server = server;
	output->damage = wlr_output_damage_create(wlr_output);
	wl_list_insert(&server->outputs, &output->link);

	output->destroy.notify = output_destroy_notify;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);

	output->damage_frame.notify = output_damage_frame_notify;
	wl_signal_add(&output->damage->events.frame, &output->damage_frame);
	output->damage_destroy.notify = output_damage_destroy_notify;
	wl_signal_add(&output->damage->events.destroy, &output->damage_destroy);

	wl_list_init(&output->layers[0]);
	wl_list_init(&output->layers[1]);
	wl_list_init(&output->layers[2]);
	wl_list_init(&output->layers[3]);
	/*
	 * Arrange outputs from left-to-right in the order they appear.
	 * TODO: support configuration in run-time
	 */
	wlr_output_layout_add_auto(server->output_layout, wlr_output);
	wlr_output_schedule_frame(wlr_output);
}

void
output_init(struct server *server)
{
	server->new_output.notify = new_output_notify;
	wl_signal_add(&server->backend->events.new_output, &server->new_output);

	/*
	 * Create an output layout, which is a wlroots utility for working with
	 * an arrangement of screens in a physical layout.
	 */
	server->output_layout = wlr_output_layout_create();
	if (!server->output_layout) {
		wlr_log(WLR_ERROR, "unable to create output layout");
		exit(EXIT_FAILURE);
	}

	/* Enable screen recording with wf-recorder */
	wlr_xdg_output_manager_v1_create(server->wl_display,
		server->output_layout);

	wl_list_init(&server->outputs);
}
