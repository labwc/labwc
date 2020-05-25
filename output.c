#include "labwc.h"

struct render_data {
	struct wlr_output *output;
	struct wlr_renderer *renderer;
	struct view *view;
	struct timespec *when;
};

static void render_grab(struct output *output)
{
	if (output->server->cursor_mode == TINYWL_CURSOR_PASSTHROUGH)
		return;

	if (output->server->cursor_mode == TINYWL_CURSOR_RESIZE) {
		float grab_box_color[] = { 0.0, 1.0, 0.0, 0.3 };
		wlr_render_rect(output->server->renderer,
				&output->server->grab_box, grab_box_color,
				output->wlr_output->transform_matrix);
	}

	float grab_point_color[] = { 1.0, 0.0, 1.0, 1.0 };
	struct wlr_box point = { .x = output->server->grab_x +
				      output->server->grabbed_view->x - 1,
				 .y = output->server->grab_y +
				      output->server->grabbed_view->y - 1,
				 .width = 3,
				 .height = 3 };
	fprintf(stderr, "XX grab_x=%f; grab_y=%f\n", output->server->grab_x,
		output->server->grab_y);
	wlr_render_rect(output->server->renderer, &point, grab_point_color,
			output->wlr_output->transform_matrix);
}

static void render_cycle_box(struct output *output)
{
	if (!output->server->cycle_view)
		return;
	struct view *view;
	wl_list_for_each_reverse (view, &output->server->views, link) {
		if (view != output->server->cycle_view)
			continue;
		struct wlr_box box = deco_max_extents(view);
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

	struct wlr_box box = deco_max_extents(view);
	float matrix[9];
	wlr_matrix_project_box(matrix, &box, WL_OUTPUT_TRANSFORM_NORMAL, 0,
			       output->transform_matrix);
	float color[] = { 0.2, 0.2, 0.7, 0.9 };
	wlr_render_quad_with_matrix(view->server->renderer, color, matrix);

	box = deco_box(view, LAB_DECO_PART_TOP);
	float color2[] = { 0.7, 0.2, 0.2, 0.9 };
	wlr_render_rect(view->server->renderer, &box, color2,
			output->transform_matrix);
}

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

	/* We also have to apply the scale factor for HiDPI outputs. This is
	 * only part of the puzzle, TinyWL does not fully support HiDPI. */
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

	/* This takes our matrix, the texture, and an alpha, and performs the
	 * actual rendering on the GPU. */
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
	render_grab(output);

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
