#include <wlr/types/wlr_xdg_output_v1.h>
#include "labwc.h"
#include "theme/theme.h"
#include "layers.h"

struct draw_data {
	struct wlr_renderer *renderer;
	float *transform_matrix;
	float *rgba;
};

static void
draw_rect(struct draw_data *d, struct wlr_box box)
{
	wlr_render_rect(d->renderer, &box, d->rgba, d->transform_matrix);
}

static void
draw_line(struct draw_data *d, int x1, int y1, int x2, int y2)
{
	struct wlr_box box = {
		.x = x1,
		.y = y1,
		.width = abs(x2 - x1) + 1,
		.height = abs(y2 - y1) + 1,
	};
	wlr_render_rect(d->renderer, &box, d->rgba, d->transform_matrix);
}

/* clang-format off */
static void draw_rect_unfilled(struct draw_data *d, struct wlr_box box)
{
	draw_line(d, box.x, box.y, box.x + box.width - 1, box.y);
	draw_line(d, box.x + box.width - 1, box.y, box.x + box.width - 1, box.y + box.height - 1);
	draw_line(d, box.x, box.y + box.height - 1, box.x + box.width - 1, box.y + box.height - 1);
	draw_line(d, box.x, box.y, box.x, box.y + box.height - 1);
}
/* clang-format on */

static void
shrink(struct wlr_box *box, int size)
{
	box->x += size;
	box->y += size;
	box->width -= 2 * size;
	box->height -= 2 * size;
}

static void
render_cycle_box(struct output *output)
{
	struct wlr_box box;
	if (!output->server->cycle_view)
		return;
	struct view *view;
	wl_list_for_each_reverse (view, &output->server->views, link) {
		if (view == output->server->cycle_view)
			goto render_it;
	}
	return;
render_it:
	box.x = view->x - view->margin.left;
	box.y = view->y - view->margin.top;
	box.width = view->w + view->margin.left + view->margin.right;
	box.height = view->h + view->margin.top + view->margin.bottom;
	struct draw_data dd = {
		.renderer = view->server->renderer,
		.transform_matrix = output->wlr_output->transform_matrix,
	};
	dd.rgba = (float[4]){ 1.0, 1.0, 1.0, 1.0 };
	draw_rect_unfilled(&dd, box);
	dd.rgba = (float[4]){ 0.0, 0.0, 0.0, 1.0 };
	for (int i = 0; i < 4; i++) {
		shrink(&box, 1);
		draw_rect_unfilled(&dd, box);
	}
	dd.rgba = (float[4]){ 1.0, 1.0, 1.0, 1.0 };
	shrink(&box, 1);
	draw_rect_unfilled(&dd, box);
}

static void
render_icon(struct draw_data *d, struct wlr_box box,
	    struct wlr_texture *texture)
{
	if (!texture)
		return;
	float matrix[9];

	/* centre-align icon if smaller than designated box */
	struct wlr_box button;
	wlr_texture_get_size(texture, &button.width, &button.height);
	if (box.width > button.width) {
		button.x = box.x + (box.width - button.width) / 2;
	} else {
		button.x = box.x;
		button.width = box.width;
	}
	if (box.height > button.height) {
		button.y = box.y + (box.height - button.height) / 2;
	} else {
		button.y = box.y;
		button.height = box.height;
	}

	wlr_matrix_project_box(matrix, &button, WL_OUTPUT_TRANSFORM_NORMAL, 0,
			       d->transform_matrix);
	wlr_render_texture_with_matrix(d->renderer, texture, matrix, 1);
}

static bool
isbutton(enum deco_part deco_part)
{
	return deco_part == LAB_DECO_BUTTON_CLOSE ||
	       deco_part == LAB_DECO_BUTTON_MAXIMIZE ||
	       deco_part == LAB_DECO_BUTTON_ICONIFY;
}

static void
render_decorations(struct wlr_output *output, struct view *view)
{
	if (!view->server_side_deco)
		return;
	struct draw_data ddata = {
		.renderer = view->server->renderer,
		.transform_matrix = output->transform_matrix,
	};

	/* border */
	ddata.rgba = theme.window_active_handle_bg_color;
	draw_rect(&ddata, deco_box(view, LAB_DECO_PART_TOP));
	draw_rect(&ddata, deco_box(view, LAB_DECO_PART_RIGHT));
	draw_rect(&ddata, deco_box(view, LAB_DECO_PART_BOTTOM));
	draw_rect(&ddata, deco_box(view, LAB_DECO_PART_LEFT));

	/* title */
	struct wlr_seat *seat = view->server->seat.seat;
	if (view->surface == seat->keyboard_state.focused_surface)
		ddata.rgba = theme.window_active_title_bg_color;
	else
		ddata.rgba = theme.window_inactive_title_bg_color;
	draw_rect(&ddata, deco_box(view, LAB_DECO_PART_TITLE));

	/* button background */
	struct wlr_cursor *cur = view->server->seat.cursor;
	enum deco_part deco_part = deco_at(view, cur->x, cur->y);

	struct wlr_box box = deco_box(view, deco_part);
	if (isbutton(deco_part) &&
	    wlr_box_contains_point(&box, cur->x, cur->y)) {
		ddata.rgba = (float[4]){ 0.5, 0.5, 0.5, 0.5 };
		draw_rect(&ddata, deco_box(view, deco_part));
	}

	/* buttons */
	if (view->surface == seat->keyboard_state.focused_surface) {
		render_icon(&ddata, deco_box(view, LAB_DECO_BUTTON_CLOSE),
			    theme.xbm_close_active_unpressed);
		render_icon(&ddata, deco_box(view, LAB_DECO_BUTTON_MAXIMIZE),
			    theme.xbm_maximize_active_unpressed);
		render_icon(&ddata, deco_box(view, LAB_DECO_BUTTON_ICONIFY),
			    theme.xbm_iconify_active_unpressed);
	} else {
		render_icon(&ddata, deco_box(view, LAB_DECO_BUTTON_CLOSE),
			    theme.xbm_close_inactive_unpressed);
		render_icon(&ddata, deco_box(view, LAB_DECO_BUTTON_MAXIMIZE),
			    theme.xbm_maximize_inactive_unpressed);
		render_icon(&ddata, deco_box(view, LAB_DECO_BUTTON_ICONIFY),
			    theme.xbm_iconify_inactive_unpressed);
	}
}

struct render_data_layer {
	struct lab_layer_surface *layer_surface;
	struct timespec *now;
};

static void
render_layer_surface(struct wlr_surface *surface, int sx, int sy, void *data)
{
	struct render_data_layer *context = data;
	struct lab_layer_surface *layer_surface = context->layer_surface;
	struct wlr_texture *texture = wlr_surface_get_texture(surface);
	if (texture == NULL) {
		return;
	}
	struct wlr_output *output = layer_surface->layer_surface->output;
	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(
			layer_surface->server->output_layout, output, &ox, &oy);
	ox += layer_surface->geo.x + sx, oy += layer_surface->geo.y + sy;
	float matrix[9];
	enum wl_output_transform transform =
		wlr_output_transform_invert(surface->current.transform);
	struct wlr_box box;
	memcpy(&box, &layer_surface->geo, sizeof(struct wlr_box));
	wlr_matrix_project_box(matrix, &box, transform, 0,
		output->transform_matrix);
	wlr_render_texture_with_matrix(layer_surface->server->renderer,
			texture, matrix, 1);
	wlr_surface_send_frame_done(surface, context->now);
}

static void
render_layer(struct timespec *now, struct wl_list *layer_surfaces)
{
	struct render_data_layer context = {
		.now = now,
	};
	struct lab_layer_surface *layer_surface;
	wl_list_for_each(layer_surface, layer_surfaces, link) {
		struct wlr_layer_surface_v1 *wlr_layer_surface_v1 =
			layer_surface->layer_surface;
		context.layer_surface = layer_surface;
		wlr_surface_for_each_surface(wlr_layer_surface_v1->surface,
			render_layer_surface, &context);
	}
}

struct render_data {
	struct wlr_output *output;
	struct wlr_output_layout *output_layout;
	struct wlr_renderer *renderer;
	int lx, ly;
	struct timespec *when;
};

static void
render_surface(struct wlr_surface *surface, int sx, int sy, void *data)
{
	struct render_data *rdata = data;

	struct wlr_texture *texture = wlr_surface_get_texture(surface);
	if (!texture) {
		return;
	}

	/*
	 * The view has a position in layout coordinates. If you have two
	 * displays, one next to the other, both 1080p, a view on the rightmost
	 * display might have layout coordinates of 2000,100. We need to
	 * translate that to output-local coordinates, or (2000 - 1920).
	 */
	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(
		rdata->output_layout, rdata->output, &ox, &oy);
	ox += rdata->lx + sx;
	oy += rdata->ly + sy;

	struct wlr_box box = {
		.x = ox * rdata->output->scale,
		.y = oy * rdata->output->scale,
		.width = surface->current.width * rdata->output->scale,
		.height = surface->current.height * rdata->output->scale,
	};

	float matrix[9];
	enum wl_output_transform transform =
		wlr_output_transform_invert(surface->current.transform);
	wlr_matrix_project_box(matrix, &box, transform, 0,
	       rdata->output->transform_matrix);

	wlr_render_texture_with_matrix(rdata->renderer, texture, matrix, 1);
	wlr_surface_send_frame_done(surface, rdata->when);
}

static void
output_frame_notify(struct wl_listener *listener, void *data)
{
	/*
	 * This function is called every time an output is ready to display a
	 * frame, generally at the output's refresh rate (e.g. 60Hz).
	 */
	struct output *output = wl_container_of(listener, output, frame);
	struct wlr_renderer *renderer = output->server->renderer;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	/* wlr_output_attach_render makes the OpenGL context current. */
	if (!wlr_output_attach_render(output->wlr_output, NULL)) {
		return;
	}

	/* The "effective" resolution can change if you rotate your outputs. */
	int width, height;
	wlr_output_effective_resolution(output->wlr_output, &width, &height);

	/* Calls glViewport and some other GL sanity checks */
	wlr_renderer_begin(renderer, width, height);

	float color[4] = { 0.3, 0.3, 0.3, 1.0 };
	wlr_renderer_clear(renderer, color);

	render_layer(&now, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
	render_layer(&now, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);

	struct view *view;
	wl_list_for_each_reverse (view, &output->server->views, link) {
		if (!view->mapped)
			continue;

		render_decorations(output->wlr_output, view);

		struct render_data rdata = {
			.output = output->wlr_output,
			.output_layout = output->server->output_layout,
			.lx = view->x,
			.ly = view->y,
			.renderer = renderer,
			.when = &now,
		};

		view_for_each_surface(view, render_surface, &rdata);
	}

	/* If in cycle (alt-tab) mode, highlight selected view */
	render_cycle_box(output);

	/* Render xwayland override_redirect surfaces */
	struct xwayland_unmanaged *unmanaged;
	wl_list_for_each_reverse (unmanaged,
				  &output->server->unmanaged_surfaces, link) {
		struct render_data rdata = {
			.output = output->wlr_output,
			.output_layout = output->server->output_layout,
			.lx = unmanaged->lx,
			.ly = unmanaged->ly,
			.renderer = renderer,
			.when = &now,
		};

		struct wlr_surface *s = unmanaged->xwayland_surface->surface;
		render_surface(s, 0, 0, &rdata);
	}

	render_layer(&now, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);
	render_layer(&now, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);
	/* Just in case hardware cursors not supported by GPU */
	wlr_output_render_software_cursors(output->wlr_output, NULL);

	wlr_renderer_end(renderer);
	wlr_output_commit(output->wlr_output);
}

static void
output_destroy_notify(struct wl_listener *listener, void *data)
{
        struct output *output = wl_container_of(listener, output, destroy);
        wl_list_remove(&output->link);
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
	output->frame.notify = output_frame_notify;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
	wl_list_insert(&server->outputs, &output->link);
	output->destroy.notify = output_destroy_notify;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);

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
