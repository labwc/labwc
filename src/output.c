#include "labwc.h"
#include "rcxml.h"
#include "theme.h"

struct draw_data {
	struct wlr_renderer *renderer;
	float *transform_matrix;
	float *rgba;
};

static void draw_rect(struct draw_data *d, struct wlr_box box)
{
	wlr_render_rect(d->renderer, &box, d->rgba, d->transform_matrix);
}

static void render_cycle_box(struct output *output)
{
	if (!output->server->cycle_view)
		return;
	struct view *view;
	wl_list_for_each_reverse (view, &output->server->views, link) {
		if (view != output->server->cycle_view)
			continue;
		struct wlr_box box;
		if ((view->type == LAB_XWAYLAND_VIEW) ||
		    !rc.client_side_decorations) {
			box = deco_max_extents(view);
		} else {
			box = view_get_surface_geometry(view);
			box.x += view->x;
			box.y += view->y;
		}
		float cycle_color[] = { 0.0, 0.0, 0.0, 0.2 };
		wlr_render_rect(output->server->renderer, &box, cycle_color,
				output->wlr_output->transform_matrix);
		return;
	}
}

static void render_decorations(struct wlr_output *output, struct view *view)
{
	if (!view_want_deco(view))
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

	ddata.rgba = theme.window_active_title_bg_color;
	draw_rect(&ddata, deco_box(view, LAB_DECO_PART_TITLE));
}

struct render_data {
	struct wlr_output *output;
	struct wlr_renderer *renderer;
	struct view *view;
	struct timespec *when;
};

static void render_surface(struct wlr_surface *surface, int sx, int sy,
			   void *data)
{
	/* This function is called for every surface that needs to be rendered.
	 */
	struct render_data *rdata = data;
	struct view *view = rdata->view;
	struct wlr_output *output = rdata->output;

	/* We first obtain a wlr_texture, which is a GPU resource. wlroots
	 * automatically handles negotiating these with the client. The
	 * underlying resource could be an opaque handle passed from the client,
	 * or the client could have sent a pixel buffer which we copied to the
	 * GPU, or a few other means. You don't have to worry about this,
	 * wlroots takes care of it. */
	struct wlr_texture *texture = wlr_surface_get_texture(surface);
	if (texture == NULL) {
		return;
	}

	/* The view has a position in layout coordinates. If you have two
	 * displays, one next to the other, both 1080p, a view on the rightmost
	 * display might have layout coordinates of 2000,100. We need to
	 * translate that to output-local coordinates, or (2000 - 1920). */
	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(view->server->output_layout, output,
					&ox, &oy);
	ox += view->x + sx;
	oy += view->y + sy;

	/* TODO: Support HiDPI */
	struct wlr_box box = {
		.x = ox * output->scale,
		.y = oy * output->scale,
		.width = surface->current.width * output->scale,
		.height = surface->current.height * output->scale,
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
			       output->transform_matrix);

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
		if (!view->mapped) {
			/* An unmapped view should not be rendered. */
			continue;
		}
		struct render_data rdata = {
			.output = output->wlr_output,
			.view = view,
			.renderer = renderer,
			.when = &now,
		};

		render_decorations(output->wlr_output, view);

		/* This calls our render_surface function for each surface among
		 * the xdg_surface's toplevel and popups. */
		if (view->type == LAB_XDG_SHELL_VIEW) {
			wlr_xdg_surface_for_each_surface(
				view->xdg_surface, render_surface, &rdata);
		} else if (view->type == LAB_XWAYLAND_VIEW) {
			render_surface(view->xwayland_surface->surface, 0, 0,
				       &rdata);
		}
	}

	/* If in cycle (alt-tab) mode, highlight selected view */
	render_cycle_box(output);

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
