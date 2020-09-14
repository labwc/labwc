#include "labwc.h"
#include "theme/theme.h"

struct draw_data {
	struct wlr_renderer *renderer;
	float *transform_matrix;
	float *rgba;
};

static void draw_rect(struct draw_data *d, struct wlr_box box)
{
	wlr_render_rect(d->renderer, &box, d->rgba, d->transform_matrix);
}

static void draw_line(struct draw_data *d, int x1, int y1, int x2, int y2)
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

static void shrink(struct wlr_box *box, int size)
{
	box->x += size;
	box->y += size;
	box->width -= 2 * size;
	box->height -= 2 * size;
}

static void render_cycle_box(struct output *output)
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
	if ((view->type == LAB_XWAYLAND_VIEW) || !rc.client_side_decorations) {
		box = deco_max_extents(view);
	} else {
		box.x = view->x;
		box.y = view->y;
		box.width = view->w;
		box.height = view->h;
	}
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

static void render_icon(struct draw_data *d, struct wlr_box box,
			struct wlr_texture *texture)
{
	if (!texture)
		return;
	float matrix[9];
	wlr_matrix_project_box(matrix, &box, WL_OUTPUT_TRANSFORM_NORMAL, 0,
			       d->transform_matrix);
	wlr_render_texture_with_matrix(d->renderer, texture, matrix, 1);
}

static void render_decorations(struct wlr_output *output, struct view *view)
{
	if (!view->show_server_side_deco)
		return;
	struct draw_data ddata = {
		.renderer = view->server->renderer,
		.transform_matrix = output->transform_matrix,
	};

	ddata.rgba = theme.window_active_handle_bg_color;
	draw_rect(&ddata, deco_box(view, LAB_DECO_PART_TOP));
	draw_rect(&ddata, deco_box(view, LAB_DECO_PART_RIGHT));
	draw_rect(&ddata, deco_box(view, LAB_DECO_PART_BOTTOM));
	draw_rect(&ddata, deco_box(view, LAB_DECO_PART_LEFT));

	if (view->surface == seat_focused_surface())
		ddata.rgba = theme.window_active_title_bg_color;
	else
		ddata.rgba = theme.window_inactive_title_bg_color;
	draw_rect(&ddata, deco_box(view, LAB_DECO_PART_TITLE));

	if (view->surface == seat_focused_surface()) {
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

struct render_data {
	struct wlr_output *output;
	struct wlr_output_layout *output_layout;
	struct wlr_renderer *renderer;
	int lx, ly;
	struct timespec *when;
};

static void render_surface(struct wlr_surface *surface, int sx, int sy,
			   void *data)
{
	struct render_data *rdata = data;

	struct wlr_texture *texture = wlr_surface_get_texture(surface);
	if (!texture)
		return;

	/* The view has a position in layout coordinates. If you have two
	 * displays, one next to the other, both 1080p, a view on the rightmost
	 * display might have layout coordinates of 2000,100. We need to
	 * translate that to output-local coordinates, or (2000 - 1920). */
	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(rdata->output_layout, rdata->output,
					&ox, &oy);
	ox += rdata->lx + sx;
	oy += rdata->ly + sy;

	/* TODO: Support HiDPI */
	struct wlr_box box = {
		.x = ox * rdata->output->scale,
		.y = oy * rdata->output->scale,
		.width = surface->current.width * rdata->output->scale,
		.height = surface->current.height * rdata->output->scale,
	};

	/*
	 * Those familiar with OpenGL are also familiar with the role of
	 * matricies in graphics programming. We need to prepare a matrix to
	 * render the view with. wlr_matrix_project_box is a helper which takes
	 * a box with a desired x, y coordinates, width and height, and an
	 * output geometry, then prepares an orthographic projection and
	 * multiplies the necessary transforms to produce a
	 * model-view-projection matrix.
	 *
	 * Naturally you can do this any way you like, for example to make a 3D
	 * compositor.
	 */
	float matrix[9];
	enum wl_output_transform transform =
		wlr_output_transform_invert(surface->current.transform);
	wlr_matrix_project_box(matrix, &box, transform, 0,
			       rdata->output->transform_matrix);

	/*
	 * This takes our matrix, the texture, and an alpha, and performs the
	 * actual rendering on the GPU.
	 */
	wlr_render_texture_with_matrix(rdata->renderer, texture, matrix, 1);

	/* This lets the client know that we've displayed that frame and it can
	 * prepare another one now if it likes. */
	wlr_surface_send_frame_done(surface, rdata->when);
}

void output_frame(struct wl_listener *listener, void *data)
{
	/* This function is called every time an output is ready to display a
	 * frame, generally at the output's refresh rate (e.g. 60Hz). */
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
	/* Begin the renderer (calls glViewport and some other GL sanity checks)
	 */
	wlr_renderer_begin(renderer, width, height);

	float color[4] = { 0.3, 0.3, 0.3, 1.0 };
	wlr_renderer_clear(renderer, color);

	/* Each subsequent window we render is rendered on top of the last.
	 * Because our view list is ordered front-to-back, we iterate over it
	 * backwards. */
	struct view *view;
	wl_list_for_each_reverse (view, &output->server->views, link) {
		if (!view->mapped)
			continue;

		struct render_data rdata = {
			.output = output->wlr_output,
			.output_layout = output->server->output_layout,
			.lx = view->x,
			.ly = view->y,
			.renderer = renderer,
			.when = &now,
		};

		render_decorations(output->wlr_output, view);

		if (view->type == LAB_XDG_SHELL_VIEW) {
			/* render each xdg toplevel and popup surface */
			wlr_xdg_surface_for_each_surface(
				view->xdg_surface, render_surface, &rdata);
		} else if (view->type == LAB_XWAYLAND_VIEW) {
			render_surface(view->xwayland_surface->surface, 0, 0,
				       &rdata);
		}
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

	/* Hardware cursors are rendered by the GPU on a separate plane, and can
	 * be moved around without re-rendering what's beneath them - which is
	 * more efficient. However, not all hardware supports hardware cursors.
	 * For this reason, wlroots provides a software fallback, which we ask
	 * it to render here. wlr_cursor handles configuring hardware vs
	 * software cursors for you,
	 * and this function is a no-op when hardware cursors are in use. */
	wlr_output_render_software_cursors(output->wlr_output, NULL);

	/* Conclude rendering and swap the buffers, showing the final frame
	 * on-screen. */
	wlr_renderer_end(renderer);
	wlr_output_commit(output->wlr_output);
}

void output_new(struct wl_listener *listener, void *data)
{
	/* This event is rasied by the backend when a new output (aka a display
	 * or monitor) becomes available. */
	struct server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	/*
	 * Some backends don't have modes. DRM+KMS does, and we need to set a
	 * mode before we can use the output. The mode is a tuple of (width,
	 * height, refresh rate), and each monitor supports only a specific set
	 * of modes. We just pick the monitor's preferred mode.
	 * TODO: support user configuration
	 */
	if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode =
			wlr_output_preferred_mode(wlr_output);
		wlr_output_set_mode(wlr_output, mode);
		wlr_output_enable(wlr_output, true);
		if (!wlr_output_commit(wlr_output)) {
			return;
		}
	}

	/* Allocates and configures our state for this output */
	struct output *output = calloc(1, sizeof(struct output));
	output->wlr_output = wlr_output;
	output->server = server;
	/* Sets up a listener for the frame notify event. */
	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
	wl_list_insert(&server->outputs, &output->link);

	/* Adds this to the output layout. The add_auto function arranges
	 * outputs from left-to-right in the order they appear. A more
	 * sophisticated compositor would let the user configure the arrangement
	 * of outputs in the layout.
	 *
	 * The output layout utility automatically adds a wl_output global to
	 * the display, which Wayland clients can see to find out information
	 * about the output (such as DPI, scale factor, manufacturer, etc).
	 */
	wlr_output_layout_add_auto(server->output_layout, wlr_output);
}
