// SPDX-License-Identifier: GPL-2.0-only
/*
 * Based on https://github.com/swaywm/sway/tree/master/swaynag
 *
 * Copyright (C) 2016-2017 Drew DeVault
 * Copyright (C) 2025 Johan Malm
 */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <cairo.h>
#include <ctype.h>
#include <getopt.h>
#include <glib.h>
#include <pango/pangocairo.h>
#include <poll.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef __FreeBSD__
#include <sys/event.h> /* For signalfd() */
#endif
#include <sys/mman.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wayland-cursor.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include "action-prompt-codes.h"
#include "pool-buffer.h"
#include "cursor-shape-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#define LABNAG_MAX_HEIGHT 500

struct conf {
	PangoFontDescription *font_description;
	char *output;
	uint32_t anchors;
	int32_t layer; /* enum zwlr_layer_shell_v1_layer or -1 if unset */
	enum zwlr_layer_surface_v1_keyboard_interactivity keyboard_focus;

	/* Colors */
	uint32_t button_text;
	uint32_t button_background;
	uint32_t details_background;
	uint32_t background;
	uint32_t text;
	uint32_t button_border;
	uint32_t border_bottom;

	/* Sizing */
	ssize_t bar_border_thickness;
	ssize_t message_padding;
	ssize_t details_border_thickness;
	ssize_t button_border_thickness;
	ssize_t button_gap;
	ssize_t button_gap_close;
	ssize_t button_margin_right;
	ssize_t button_padding;
};

struct pointer {
	struct wl_pointer *pointer;
	uint32_t serial;
	struct wl_cursor_theme *cursor_theme;
	struct wl_cursor_image *cursor_image;
	struct wl_surface *cursor_surface;
	int x;
	int y;
};

struct keyboard {
	struct wl_keyboard *keyboard;
	struct xkb_keymap *keymap;
	struct xkb_state *state;
};

struct seat {
	struct wl_seat *wl_seat;
	uint32_t wl_name;
	struct nag *nag;
	struct pointer pointer;
	struct keyboard keyboard;
	struct wl_list link; /* nag.seats */
};

struct output {
	char *name;
	struct wl_output *wl_output;
	uint32_t wl_name;
	uint32_t scale;
	struct nag *nag;
	struct wl_list link; /* nag.outputs */
};

struct button {
	char *text;
	char *action;
	int x;
	int y;
	int width;
	int height;
	bool expand;
	bool dismiss;
	struct wl_list link;
};

enum {
	FD_WAYLAND,
	FD_TIMER,
	FD_SIGNAL,

	NR_FDS,
};

struct nag {
	bool run_display;

	struct wl_display *display;
	struct wl_compositor *compositor;
	struct wl_seat *seat;
	struct wl_shm *shm;
	struct wl_list outputs;
	struct wl_list seats;
	struct output *output;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct zwlr_layer_surface_v1 *layer_surface;
	struct wp_cursor_shape_manager_v1 *cursor_shape_manager;
	struct wl_surface *surface;

	uint32_t width;
	uint32_t height;
	int32_t scale;
	struct pool_buffer buffers[2];
	struct pool_buffer *current_buffer;

	struct conf *conf;
	char *message;
	struct wl_list buttons;
	int selected_button;
	struct pollfd pollfds[NR_FDS];

	struct {
		bool visible;
		char *message;
		char *details_text;
		int close_timeout;
		bool use_exclusive_zone;

		int x;
		int y;
		int width;
		int height;

		int offset;
		int visible_lines;
		int total_lines;
		struct button *button_details;
		struct button button_up;
		struct button button_down;
	} details;
};

static int exit_status = LAB_EXIT_FAILURE;

static void close_pollfd(struct pollfd *pollfd);

static PangoLayout *
get_pango_layout(cairo_t *cairo, const PangoFontDescription *desc,
		const char *text, double scale, bool markup)
{
	PangoLayout *layout = pango_cairo_create_layout(cairo);
	pango_context_set_round_glyph_positions(pango_layout_get_context(layout), false);

	PangoAttrList *attrs;
	if (markup) {
		char *buf;
		GError *error = NULL;
		if (pango_parse_markup(text, -1, 0, &attrs, &buf, NULL, &error)) {
			pango_layout_set_text(layout, buf, -1);
			free(buf);
		} else {
			wlr_log(WLR_ERROR, "pango_parse_markup '%s' -> error %s",
				text, error->message);
			g_error_free(error);
			markup = false; /* fallback to plain text */
		}
	}
	if (!markup) {
		attrs = pango_attr_list_new();
		pango_layout_set_text(layout, text, -1);
	}

	pango_attr_list_insert(attrs, pango_attr_scale_new(scale));
	pango_layout_set_font_description(layout, desc);
	pango_layout_set_single_paragraph_mode(layout, 1);
	pango_layout_set_attributes(layout, attrs);
	pango_attr_list_unref(attrs);
	return layout;
}

static void
get_text_size(cairo_t *cairo, const PangoFontDescription *desc, int *width, int *height,
		int *baseline, double scale, bool markup, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	gchar *buf = g_strdup_vprintf(fmt, args);
	va_end(args);
	if (!buf) {
		return;
	}

	PangoLayout *layout = get_pango_layout(cairo, desc, buf, scale, markup);
	pango_cairo_update_layout(cairo, layout);
	pango_layout_get_pixel_size(layout, width, height);
	if (baseline) {
		*baseline = pango_layout_get_baseline(layout) / PANGO_SCALE;
	}
	g_object_unref(layout);

	g_free(buf);
}

static void
render_text(cairo_t *cairo, const PangoFontDescription *desc, double scale,
		bool markup, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	gchar *buf = g_strdup_vprintf(fmt, args);
	va_end(args);
	if (!buf) {
		return;
	}

	PangoLayout *layout = get_pango_layout(cairo, desc, buf, scale, markup);
	cairo_font_options_t *fo = cairo_font_options_create();
	cairo_get_font_options(cairo, fo);
	pango_cairo_context_set_font_options(pango_layout_get_context(layout), fo);
	cairo_font_options_destroy(fo);
	pango_cairo_update_layout(cairo, layout);
	pango_cairo_show_layout(cairo, layout);
	g_object_unref(layout);

	g_free(buf);
}

static void
cairo_set_source_u32(cairo_t *cairo, uint32_t color)
{
	cairo_set_source_rgba(cairo,
			(color >> (3*8) & 0xFF) / 255.0,
			(color >> (2*8) & 0xFF) / 255.0,
			(color >> (1*8) & 0xFF) / 255.0,
			(color >> (0*8) & 0xFF) / 255.0);
}

static uint32_t
render_message(cairo_t *cairo, struct nag *nag)
{
	int text_width, text_height;
	get_text_size(cairo, nag->conf->font_description, &text_width,
		&text_height, NULL, 1, true, "%s", nag->message);

	int padding = nag->conf->message_padding;

	uint32_t ideal_height = text_height + padding * 2;
	uint32_t ideal_surface_height = ideal_height;
	if (nag->height < ideal_surface_height) {
		return ideal_surface_height;
	}

	cairo_set_source_u32(cairo, nag->conf->text);
	cairo_move_to(cairo, padding, (int)(ideal_height - text_height) / 2);
	render_text(cairo, nag->conf->font_description, 1, false,
			"%s", nag->message);

	return ideal_surface_height;
}

static void
render_details_scroll_button(cairo_t *cairo, struct nag *nag,
		struct button *button)
{
	int text_width, text_height;
	get_text_size(cairo, nag->conf->font_description, &text_width,
		&text_height, NULL, 1, true, "%s", button->text);

	int border = nag->conf->button_border_thickness;
	int padding = nag->conf->button_padding;

	cairo_set_source_u32(cairo, nag->conf->details_background);
	cairo_rectangle(cairo, button->x, button->y,
			button->width, button->height);
	cairo_fill(cairo);

	cairo_set_source_u32(cairo, nag->conf->button_background);
	cairo_rectangle(cairo, button->x + border, button->y + border,
			button->width - (border * 2),
			button->height - (border * 2));
	cairo_fill(cairo);

	cairo_set_source_u32(cairo, nag->conf->button_text);
	cairo_move_to(cairo, button->x + border + padding,
			button->y + border + (button->height - text_height) / 2);
	render_text(cairo, nag->conf->font_description, 1, true,
			"%s", button->text);
}

static int
get_detailed_scroll_button_width(cairo_t *cairo, struct nag *nag)
{
	int up_width, down_width, temp_height;
	get_text_size(cairo, nag->conf->font_description, &up_width, &temp_height,
		NULL, 1, true, "%s", nag->details.button_up.text);
	get_text_size(cairo, nag->conf->font_description, &down_width, &temp_height,
		NULL, 1, true, "%s", nag->details.button_down.text);

	int text_width =  up_width > down_width ? up_width : down_width;
	int border = nag->conf->button_border_thickness;
	int padding = nag->conf->button_padding;

	return text_width + border * 2 + padding * 2;
}

static uint32_t
render_detailed(cairo_t *cairo, struct nag *nag, uint32_t y)
{
	uint32_t width = nag->width;

	int border = nag->conf->details_border_thickness;
	int padding = nag->conf->message_padding;
	int decor = padding + border;

	nag->details.x = decor;
	nag->details.y = y + decor;
	nag->details.width = width - decor * 2;

	PangoLayout *layout = get_pango_layout(cairo, nag->conf->font_description,
			nag->details.message, 1, false);
	pango_layout_set_width(layout,
			(nag->details.width - padding * 2) * PANGO_SCALE);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
	pango_layout_set_single_paragraph_mode(layout, false);
	pango_cairo_update_layout(cairo, layout);
	nag->details.total_lines = pango_layout_get_line_count(layout);

	PangoLayoutLine *line;
	line = pango_layout_get_line_readonly(layout, nag->details.offset);
	gint offset = line->start_index;
	const char *text = pango_layout_get_text(layout);
	pango_layout_set_text(layout, text + offset, strlen(text) - offset);

	int text_width, text_height;
	pango_cairo_update_layout(cairo, layout);
	pango_layout_get_pixel_size(layout, &text_width, &text_height);

	bool show_buttons = nag->details.offset > 0;
	int button_width = get_detailed_scroll_button_width(cairo, nag);
	if (show_buttons) {
		nag->details.width -= button_width;
		pango_layout_set_width(layout,
				(nag->details.width - padding * 2) * PANGO_SCALE);
	}

	uint32_t ideal_height;
	do {
		ideal_height = nag->details.y + text_height + decor + padding * 2;
		if (ideal_height > LABNAG_MAX_HEIGHT) {
			ideal_height = LABNAG_MAX_HEIGHT;

			if (!show_buttons) {
				show_buttons = true;
				nag->details.width -= button_width;
				pango_layout_set_width(layout,
					(nag->details.width - padding * 2) * PANGO_SCALE);
			}
		}

		nag->details.height = ideal_height - nag->details.y - decor;
		pango_layout_set_height(layout,
				(nag->details.height - padding * 2) * PANGO_SCALE);
		pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
		pango_cairo_update_layout(cairo, layout);
		pango_layout_get_pixel_size(layout, &text_width, &text_height);
	} while (text_height != (nag->details.height - padding * 2));

	nag->details.visible_lines = pango_layout_get_line_count(layout);

	if (show_buttons) {
		nag->details.button_up.x = nag->details.x + nag->details.width;
		nag->details.button_up.y = nag->details.y;
		nag->details.button_up.width = button_width;
		nag->details.button_up.height = nag->details.height / 2;
		render_details_scroll_button(cairo, nag, &nag->details.button_up);

		nag->details.button_down.x = nag->details.x + nag->details.width;
		nag->details.button_down.y =
			nag->details.button_up.y + nag->details.button_up.height;
		nag->details.button_down.width = button_width;
		nag->details.button_down.height = nag->details.height / 2;
		render_details_scroll_button(cairo, nag, &nag->details.button_down);
	}

	cairo_set_source_u32(cairo, nag->conf->details_background);
	cairo_rectangle(cairo, nag->details.x, nag->details.y,
			nag->details.width, nag->details.height);
	cairo_fill(cairo);

	cairo_move_to(cairo, nag->details.x + padding, nag->details.y + padding);
	cairo_set_source_u32(cairo, nag->conf->text);
	pango_cairo_show_layout(cairo, layout);
	g_object_unref(layout);

	return ideal_height;
}

static uint32_t
render_button(cairo_t *cairo, struct nag *nag, struct button *button,
		bool selected, int *x)
{
	int text_width, text_height;
	get_text_size(cairo, nag->conf->font_description, &text_width,
		&text_height, NULL, 1, true, "%s", button->text);

	int border = nag->conf->button_border_thickness;
	int padding = nag->conf->button_padding;

	uint32_t ideal_height = text_height + padding * 2 + border * 2;
	uint32_t ideal_surface_height = ideal_height;
	if (nag->height < ideal_surface_height) {
		return ideal_surface_height;
	}

	button->x = *x - border - text_width - padding * 2 + 1;
	button->y = (int)(ideal_height - text_height) / 2 - padding + 1;
	button->width = text_width + padding * 2;
	button->height = text_height + padding * 2;

	cairo_set_source_u32(cairo, nag->conf->button_border);
	cairo_rectangle(cairo, button->x - border, button->y - border,
			button->width + border * 2, button->height + border * 2);
	cairo_fill(cairo);

	cairo_set_source_u32(cairo, nag->conf->button_background);
	cairo_rectangle(cairo, button->x, button->y,
			button->width, button->height);
	cairo_fill(cairo);

	if (selected) {
		cairo_set_source_u32(cairo, nag->conf->button_border);
		cairo_set_line_width(cairo, 1);
		cairo_rectangle(cairo, button->x + 1.5, button->y + 1.5,
			button->width - 3, button->height - 3);
		cairo_stroke(cairo);
	}

	cairo_set_source_u32(cairo, nag->conf->button_text);
	cairo_move_to(cairo, button->x + padding, button->y + padding);
	render_text(cairo, nag->conf->font_description, 1, true,
			"%s", button->text);

	*x = button->x - border;

	return ideal_surface_height;
}

static uint32_t
render_to_cairo(cairo_t *cairo, struct nag *nag)
{
	uint32_t max_height = 0;

	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_u32(cairo, nag->conf->background);
	cairo_paint(cairo);

	uint32_t h = render_message(cairo, nag);
	max_height = h > max_height ? h : max_height;

	int x = nag->width - nag->conf->button_margin_right;
	x -= nag->conf->button_gap_close;

	int idx = 0;
	struct button *button;
	wl_list_for_each(button, &nag->buttons, link) {
		h = render_button(cairo, nag, button, idx == nag->selected_button, &x);
		max_height = h > max_height ? h : max_height;
		x -= nag->conf->button_gap;
		idx++;
	}

	if (nag->details.visible) {
		h = render_detailed(cairo, nag, max_height);
		max_height = h > max_height ? h : max_height;
	}

	int border = nag->conf->bar_border_thickness;
	if (max_height > nag->height) {
		max_height += border;
	}
	cairo_set_source_u32(cairo, nag->conf->border_bottom);
	cairo_rectangle(cairo, 0,
			nag->height - border,
			nag->width,
			border);
	cairo_fill(cairo);

	return max_height;
}

static void
render_frame(struct nag *nag)
{
	if (!nag->run_display) {
		return;
	}

	cairo_surface_t *recorder = cairo_recording_surface_create(
			CAIRO_CONTENT_COLOR_ALPHA, NULL);
	cairo_t *cairo = cairo_create(recorder);
	cairo_scale(cairo, nag->scale, nag->scale);
	cairo_save(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cairo);
	cairo_restore(cairo);
	uint32_t height = render_to_cairo(cairo, nag);
	if (height != nag->height) {
		zwlr_layer_surface_v1_set_size(nag->layer_surface, 0, height);
		if (nag->details.use_exclusive_zone) {
			zwlr_layer_surface_v1_set_exclusive_zone(
				nag->layer_surface, height);
		}
		wl_surface_commit(nag->surface);
		wl_display_roundtrip(nag->display);
	} else {
		nag->current_buffer = get_next_buffer(nag->shm,
				nag->buffers,
				nag->width * nag->scale,
				nag->height * nag->scale);
		if (!nag->current_buffer) {
			wlr_log(WLR_DEBUG, "Failed to get buffer. Skipping frame.");
			goto cleanup;
		}

		cairo_t *shm = nag->current_buffer->cairo;
		cairo_save(shm);
		cairo_set_operator(shm, CAIRO_OPERATOR_CLEAR);
		cairo_paint(shm);
		cairo_restore(shm);
		cairo_set_source_surface(shm, recorder, 0.0, 0.0);
		cairo_paint(shm);

		wl_surface_set_buffer_scale(nag->surface, nag->scale);
		wl_surface_attach(nag->surface,
				nag->current_buffer->buffer, 0, 0);
		wl_surface_damage(nag->surface, 0, 0,
				nag->width, nag->height);
		wl_surface_commit(nag->surface);
		wl_display_roundtrip(nag->display);
	}

cleanup:
	cairo_surface_destroy(recorder);
	cairo_destroy(cairo);
}

static void
seat_destroy(struct seat *seat)
{
	if (seat->pointer.cursor_theme) {
		wl_cursor_theme_destroy(seat->pointer.cursor_theme);
	}
	if (seat->pointer.pointer) {
		wl_pointer_destroy(seat->pointer.pointer);
	}
	if (seat->keyboard.keyboard) {
		wl_keyboard_destroy(seat->keyboard.keyboard);
	}
	if (seat->keyboard.keymap) {
		xkb_keymap_unref(seat->keyboard.keymap);
	}
	if (seat->keyboard.state) {
		xkb_state_unref(seat->keyboard.state);
	}
	wl_seat_destroy(seat->wl_seat);
	wl_list_remove(&seat->link);
	free(seat);
}

static void
nag_destroy(struct nag *nag)
{
	nag->run_display = false;

	struct button *button, *next;
	wl_list_for_each_safe(button, next, &nag->buttons, link) {
		wl_list_remove(&button->link);
		free(button);
	}
	free(nag->details.message);

	pango_font_description_free(nag->conf->font_description);

	if (nag->layer_surface) {
		zwlr_layer_surface_v1_destroy(nag->layer_surface);
	}

	if (nag->surface) {
		wl_surface_destroy(nag->surface);
	}

	if (nag->layer_shell) {
		zwlr_layer_shell_v1_destroy(nag->layer_shell);
	}

	if (nag->cursor_shape_manager) {
		wp_cursor_shape_manager_v1_destroy(nag->cursor_shape_manager);
	}

	struct seat *seat, *tmpseat;
	wl_list_for_each_safe(seat, tmpseat, &nag->seats, link) {
		seat_destroy(seat);
	}

	destroy_buffer(&nag->buffers[0]);
	destroy_buffer(&nag->buffers[1]);

	if (nag->outputs.prev || nag->outputs.next) {
		struct output *output, *temp;
		wl_list_for_each_safe(output, temp, &nag->outputs, link) {
			wl_output_destroy(output->wl_output);
			free(output->name);
			wl_list_remove(&output->link);
			free(output);
		};
	}

	if (nag->compositor) {
		wl_compositor_destroy(nag->compositor);
	}

	if (nag->shm) {
		wl_shm_destroy(nag->shm);
	}

	if (nag->display) {
		wl_display_disconnect(nag->display);
	}
	pango_cairo_font_map_set_default(NULL);

	close_pollfd(&nag->pollfds[FD_TIMER]);
	close_pollfd(&nag->pollfds[FD_SIGNAL]);
}

static void
button_execute(struct nag *nag, struct button *button)
{
	wlr_log(WLR_DEBUG, "Executing [%s]: %s", button->text, button->action);
	if (button->expand) {
		nag->details.visible = !nag->details.visible;
		render_frame(nag);
		return;
	}
	if (button->dismiss) {
		nag->run_display = false;
	}
	if (button->action) {
		pid_t pid = fork();
		if (pid < 0) {
			wlr_log_errno(WLR_DEBUG, "Failed to fork");
			return;
		} else if (pid == 0) {
			/*
			 * Child process. Will be used to prevent zombie
			 * processes
			 */
			pid = fork();
			if (pid < 0) {
				wlr_log_errno(WLR_DEBUG, "Failed to fork");
				return;
			} else if (pid == 0) {
				/*
				 * Child of the child. Will be reparented to the
				 * init process
				 */
				execlp("sh", "sh", "-c", button->action, NULL);
				wlr_log_errno(WLR_DEBUG, "execlp failed");
				_exit(LAB_EXIT_FAILURE);
			}
			_exit(EXIT_SUCCESS);
		}

		if (waitpid(pid, NULL, 0) < 0) {
			wlr_log_errno(WLR_DEBUG, "waitpid failed");
		}
	}
}

static void
layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *surface,
		uint32_t serial, uint32_t width, uint32_t height)
{
	struct nag *nag = data;
	nag->width = width;
	nag->height = height;
	zwlr_layer_surface_v1_ack_configure(surface, serial);
	render_frame(nag);
}

static void
layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *surface)
{
	struct nag *nag = data;
	nag_destroy(nag);
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

static void
surface_enter(void *data, struct wl_surface *surface, struct wl_output *output)
{
	struct nag *nag = data;
	struct output *nag_output;
	wl_list_for_each(nag_output, &nag->outputs, link) {
		if (nag_output->wl_output == output) {
			wlr_log(WLR_DEBUG, "Surface enter on output %s",
					nag_output->name);
			nag->output = nag_output;
			nag->scale = nag->output->scale;
			render_frame(nag);
			break;
		}
	}
}

static void
surface_leave(void *data, struct wl_surface *wl_surface, struct wl_output *output)
{
	/* nop */
}

static const struct wl_surface_listener surface_listener = {
	.enter = surface_enter,
	.leave = surface_leave,
};

static void
update_cursor(struct seat *seat)
{
	struct pointer *pointer = &seat->pointer;
	struct nag *nag = seat->nag;
	if (pointer->cursor_theme) {
		wl_cursor_theme_destroy(pointer->cursor_theme);
	}
	const char *cursor_theme = getenv("XCURSOR_THEME");
	unsigned int cursor_size = 24;
	const char *env_cursor_size = getenv("XCURSOR_SIZE");
	if (env_cursor_size && *env_cursor_size) {
		errno = 0;
		char *end;
		unsigned int size = strtoul(env_cursor_size, &end, 10);
		if (!*end && errno == 0) {
			cursor_size = size;
		}
	}
	pointer->cursor_theme = wl_cursor_theme_load(
		cursor_theme, cursor_size * nag->scale, nag->shm);
	if (!pointer->cursor_theme) {
		wlr_log(WLR_ERROR, "Failed to load cursor theme");
		return;
	}
	struct wl_cursor *cursor =
		wl_cursor_theme_get_cursor(pointer->cursor_theme, "default");
	if (!cursor) {
		wlr_log(WLR_ERROR, "Failed to get default cursor from theme");
		return;
	}
	pointer->cursor_image = cursor->images[0];
	wl_surface_set_buffer_scale(pointer->cursor_surface,
			nag->scale);
	wl_surface_attach(pointer->cursor_surface,
			wl_cursor_image_get_buffer(pointer->cursor_image), 0, 0);
	wl_pointer_set_cursor(pointer->pointer, pointer->serial,
			pointer->cursor_surface,
			pointer->cursor_image->hotspot_x / nag->scale,
			pointer->cursor_image->hotspot_y / nag->scale);
	wl_surface_damage_buffer(pointer->cursor_surface, 0, 0,
			INT32_MAX, INT32_MAX);
	wl_surface_commit(pointer->cursor_surface);
}

static void
update_all_cursors(struct nag *nag)
{
	struct seat *seat;
	wl_list_for_each(seat, &nag->seats, link) {
		if (seat->pointer.pointer) {
			update_cursor(seat);
		}
	}
}

static void
wl_pointer_enter(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
		struct wl_surface *surface, wl_fixed_t surface_x,
		wl_fixed_t surface_y)
{
	struct seat *seat = data;

	struct pointer *pointer = &seat->pointer;
	pointer->x = wl_fixed_to_int(surface_x);
	pointer->y = wl_fixed_to_int(surface_y);

	if (seat->nag->cursor_shape_manager) {
		struct wp_cursor_shape_device_v1 *device =
			wp_cursor_shape_manager_v1_get_pointer(
				seat->nag->cursor_shape_manager, wl_pointer);
		wp_cursor_shape_device_v1_set_shape(device, serial,
			WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
		wp_cursor_shape_device_v1_destroy(device);
	} else {
		pointer->serial = serial;
		update_cursor(seat);
	}
}

static void
wl_pointer_leave(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface)
{
	/* nop */
}

static void
wl_pointer_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time,
		wl_fixed_t surface_x, wl_fixed_t surface_y)
{
	struct seat *seat = data;
	seat->pointer.x = wl_fixed_to_int(surface_x);
	seat->pointer.y = wl_fixed_to_int(surface_y);
}

static void
wl_pointer_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
		uint32_t time, uint32_t button, uint32_t state)
{
	struct seat *seat = data;
	struct nag *nag = seat->nag;

	if (state != WL_POINTER_BUTTON_STATE_PRESSED) {
		return;
	}

	double x = seat->pointer.x;
	double y = seat->pointer.y;

	int index = 0;
	struct button *nagbutton;
	wl_list_for_each(nagbutton, &nag->buttons, link) {
		if (x >= nagbutton->x
				&& y >= nagbutton->y
				&& x < nagbutton->x + nagbutton->width
				&& y < nagbutton->y + nagbutton->height) {
			button_execute(nag, nagbutton);
			exit_status = index;
			return;
		}
		++index;
	}

	if (nag->details.visible &&
			nag->details.total_lines != nag->details.visible_lines) {
		struct button button_up = nag->details.button_up;
		if (x >= button_up.x
				&& y >= button_up.y
				&& x < button_up.x + button_up.width
				&& y < button_up.y + button_up.height
				&& nag->details.offset > 0) {
			nag->details.offset--;
			render_frame(nag);
			return;
		}

		struct button button_down = nag->details.button_down;
		int bot = nag->details.total_lines;
		bot -= nag->details.visible_lines;
		if (x >= button_down.x
				&& y >= button_down.y
				&& x < button_down.x + button_down.width
				&& y < button_down.y + button_down.height
				&& nag->details.offset < bot) {
			nag->details.offset++;
			render_frame(nag);
			return;
		}
	}
}

static void
wl_pointer_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time,
		uint32_t axis, wl_fixed_t value)
{
	struct seat *seat = data;
	struct nag *nag = seat->nag;
	if (!nag->details.visible
			|| seat->pointer.x < nag->details.x
			|| seat->pointer.y < nag->details.y
			|| seat->pointer.x >= nag->details.x + nag->details.width
			|| seat->pointer.y >= nag->details.y + nag->details.height
			|| nag->details.total_lines == nag->details.visible_lines) {
		return;
	}

	int direction = wl_fixed_to_int(value);
	int bot = nag->details.total_lines - nag->details.visible_lines;
	if (direction < 0 && nag->details.offset > 0) {
		nag->details.offset--;
	} else if (direction > 0 && nag->details.offset < bot) {
		nag->details.offset++;
	}

	render_frame(nag);
}

static void
wl_pointer_frame(void *data, struct wl_pointer *wl_pointer)
{
	struct seat *seat = data;
	/* pointer inputs clears timer for auto-closing */
	close_pollfd(&seat->nag->pollfds[FD_TIMER]);
}

static void
wl_pointer_axis_source(void *data, struct wl_pointer *wl_pointer,
		uint32_t axis_source)
{
	/* nop */
}

static void
wl_pointer_axis_stop(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, uint32_t axis)
{
	/* nop */
}

static void
wl_pointer_axis_discrete(void *data, struct wl_pointer *wl_pointer,
		uint32_t axis, int32_t discrete)
{
	/* nop */
}

static const struct wl_pointer_listener pointer_listener = {
	.enter = wl_pointer_enter,
	.leave = wl_pointer_leave,
	.motion = wl_pointer_motion,
	.button = wl_pointer_button,
	.axis = wl_pointer_axis,
	.frame = wl_pointer_frame,
	.axis_source = wl_pointer_axis_source,
	.axis_stop = wl_pointer_axis_stop,
	.axis_discrete = wl_pointer_axis_discrete,
};

static void
wl_keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t format, int32_t fd, uint32_t size)
{
	struct seat *seat = data;

	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		wlr_log(WLR_ERROR, "unreconizable keymap format: %d", format);
		close(fd);
		return;
	}

	char *map_shm = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map_shm == MAP_FAILED) {
		wlr_log_errno(WLR_ERROR, "mmap()");
		close(fd);
		return;
	}

	if (seat->keyboard.keymap) {
		xkb_keymap_unref(seat->keyboard.keymap);
		seat->keyboard.keymap = NULL;
	}
	if (seat->keyboard.state) {
		xkb_state_unref(seat->keyboard.state);
		seat->keyboard.state = NULL;
	}
	struct xkb_context *xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	seat->keyboard.keymap = xkb_keymap_new_from_string(xkb, map_shm,
		XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (seat->keyboard.keymap) {
		seat->keyboard.state = xkb_state_new(seat->keyboard.keymap);
	} else {
		wlr_log(WLR_ERROR, "failed to compile keymap");
	}
	xkb_context_unref(xkb);

	munmap(map_shm, size);
	close(fd);
}

static void
wl_keyboard_enter(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial,
		struct wl_surface *surface, struct wl_array *keys)
{
}

static void
wl_keyboard_leave(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial,
		struct wl_surface *surface)
{
}

static void
wl_keyboard_key(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial,
		uint32_t time, uint32_t key, uint32_t state)
{
	struct seat *seat = data;
	struct nag *nag = seat->nag;

	if (!seat->keyboard.keymap || !seat->keyboard.state) {
		wlr_log(WLR_ERROR, "keymap/state unavailable");
		return;
	}

	if (state != WL_KEYBOARD_KEY_STATE_PRESSED) {
		return;
	}

	key += 8;
	const xkb_keysym_t *syms;
	if (!xkb_keymap_key_get_syms_by_level(seat->keyboard.keymap,
			key, 0, 0, &syms)) {
		wlr_log(WLR_ERROR, "failed to translate key: %d", key);
		return;
	}
	xkb_mod_mask_t mods = xkb_state_serialize_mods(seat->keyboard.state,
				XKB_STATE_MODS_EFFECTIVE);
	xkb_mod_index_t shift_idx = xkb_keymap_mod_get_index(
		seat->keyboard.keymap, XKB_MOD_NAME_SHIFT);
	bool shift = shift_idx != XKB_MOD_INVALID && (mods & (1 << shift_idx));

	int nr_buttons = wl_list_length(&nag->buttons);

	switch (syms[0]) {
	case XKB_KEY_Left:
	case XKB_KEY_Right:
	case XKB_KEY_Tab: {
		if (nr_buttons <= 0) {
			break;
		}
		int direction;
		if (syms[0] == XKB_KEY_Left || (syms[0] == XKB_KEY_Tab && shift)) {
			direction = 1;
		} else {
			direction = -1;
		}
		nag->selected_button += nr_buttons + direction;
		nag->selected_button %= nr_buttons;
		render_frame(nag);
		close_pollfd(&nag->pollfds[FD_TIMER]);
		break;
	}
	case XKB_KEY_Escape:
		exit_status = LAB_EXIT_CANCELLED;
		nag->run_display = false;
		break;
	case XKB_KEY_Return:
	case XKB_KEY_KP_Enter: {
		int idx = 0;
		struct button *button;
		wl_list_for_each(button, &nag->buttons, link) {
			if (idx == nag->selected_button) {
				button_execute(nag, button);
				close_pollfd(&nag->pollfds[FD_TIMER]);
				exit_status = idx;
				break;
			}
			idx++;
		}
		break;
	}
	}
}

static void
wl_keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
		uint32_t mods_locked, uint32_t group)
{
	struct seat *seat = data;

	if (!seat->keyboard.state) {
		wlr_log(WLR_ERROR, "xkb state unavailable");
		return;
	}

	xkb_state_update_mask(seat->keyboard.state, mods_depressed,
		mods_latched, mods_locked, 0, 0, group);
}

static void
wl_keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
		int32_t rate, int32_t delay)
{
}

static const struct wl_keyboard_listener keyboard_listener = {
	.keymap = wl_keyboard_keymap,
	.enter = wl_keyboard_enter,
	.leave = wl_keyboard_leave,
	.key = wl_keyboard_key,
	.modifiers = wl_keyboard_modifiers,
	.repeat_info = wl_keyboard_repeat_info,
};

static void
seat_handle_capabilities(void *data, struct wl_seat *wl_seat,
		enum wl_seat_capability caps)
{
	struct seat *seat = data;
	bool cap_pointer = caps & WL_SEAT_CAPABILITY_POINTER;
	bool cap_keyboard = caps & WL_SEAT_CAPABILITY_KEYBOARD;

	if (cap_pointer && !seat->pointer.pointer) {
		seat->pointer.pointer = wl_seat_get_pointer(wl_seat);
		wl_pointer_add_listener(seat->pointer.pointer,
				&pointer_listener, seat);
	} else if (!cap_pointer && seat->pointer.pointer) {
		wl_pointer_destroy(seat->pointer.pointer);
		seat->pointer.pointer = NULL;
	}

	if (cap_keyboard && !seat->keyboard.keyboard) {
		seat->keyboard.keyboard = wl_seat_get_keyboard(wl_seat);
		wl_keyboard_add_listener(seat->keyboard.keyboard,
				&keyboard_listener, seat);
	} else if (!cap_keyboard && seat->keyboard.keyboard) {
		wl_keyboard_destroy(seat->keyboard.keyboard);
		seat->keyboard.keyboard = NULL;
	}
}

static void
seat_handle_name(void *data, struct wl_seat *wl_seat, const char *name)
{
	/* nop */
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_handle_capabilities,
	.name = seat_handle_name,
};

static void
output_geometry(void *data, struct wl_output *wl_output, int32_t x, int32_t y,
		int32_t physical_width, int32_t physical_height,
		int32_t subpixel, const char *make, const char *model,
		int32_t transform)
{
	/* nop */
}

static void
output_mode(void *data, struct wl_output *wl_output, uint32_t flags,
		int32_t width, int32_t height, int32_t refresh)
{
	/* nop */
}

static void
output_done(void *data, struct wl_output *output)
{
	/* nop */
}

static void
output_scale(void *data, struct wl_output *output, int32_t factor)
{
	struct output *nag_output = data;
	nag_output->scale = factor;
	if (nag_output->nag->output == nag_output) {
		nag_output->nag->scale = nag_output->scale;
		if (!nag_output->nag->cursor_shape_manager) {
			update_all_cursors(nag_output->nag);
		}
		render_frame(nag_output->nag);
	}
}

static void
output_name(void *data, struct wl_output *output, const char *name)
{
	struct output *nag_output = data;
	nag_output->name = strdup(name);

	const char *outname = nag_output->nag->conf->output;
	if (!nag_output->nag->output && outname &&
			strcmp(outname, name) == 0) {
		wlr_log(WLR_DEBUG, "Using output %s", name);
		nag_output->nag->output = nag_output;
	}
}

static void
output_description(void *data, struct wl_output *wl_output,
		const char *description)
{
	/* nop */
}

static const struct wl_output_listener output_listener = {
	.geometry = output_geometry,
	.mode = output_mode,
	.done = output_done,
	.scale = output_scale,
	.name = output_name,
	.description = output_description,
};

static void
handle_global(void *data, struct wl_registry *registry, uint32_t name,
		const char *interface, uint32_t version)
{
	struct nag *nag = data;
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		nag->compositor = wl_registry_bind(registry, name,
				&wl_compositor_interface, 4);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		struct seat *seat = calloc(1, sizeof(*seat));
		if (!seat) {
			perror("calloc");
			return;
		}

		seat->nag = nag;
		seat->wl_name = name;
		seat->wl_seat =
			wl_registry_bind(registry, name, &wl_seat_interface, 5);

		wl_seat_add_listener(seat->wl_seat, &seat_listener, seat);

		wl_list_insert(&nag->seats, &seat->link);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		nag->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		if (!nag->output) {
			struct output *output = calloc(1, sizeof(*output));
			if (!output) {
				perror("calloc");
				return;
			}
			output->wl_output = wl_registry_bind(registry, name,
					&wl_output_interface, 4);
			output->wl_name = name;
			output->scale = 1;
			output->nag = nag;
			wl_list_insert(&nag->outputs, &output->link);
			wl_output_add_listener(output->wl_output,
					&output_listener, output);
		}
	} else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		nag->layer_shell = wl_registry_bind(
				registry, name, &zwlr_layer_shell_v1_interface, 4);
	} else if (strcmp(interface, wp_cursor_shape_manager_v1_interface.name) == 0) {
		nag->cursor_shape_manager = wl_registry_bind(
				registry, name, &wp_cursor_shape_manager_v1_interface, 1);
	}
}

static void
handle_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
	struct nag *nag = data;
	if (nag->output->wl_name == name) {
		nag->run_display = false;
	}

	struct seat *seat, *tmpseat;
	wl_list_for_each_safe(seat, tmpseat, &nag->seats, link) {
		if (seat->wl_name == name) {
			seat_destroy(seat);
		}
	}
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

static void
nag_setup_cursors(struct nag *nag)
{
	struct seat *seat;

	wl_list_for_each(seat, &nag->seats, link) {
		struct pointer *p = &seat->pointer;

		p->cursor_surface =
			wl_compositor_create_surface(nag->compositor);
		assert(p->cursor_surface);
	}
}

static void
nag_setup(struct nag *nag)
{
	nag->display = wl_display_connect(NULL);
	if (!nag->display) {
		wlr_log(WLR_ERROR, "Unable to connect to the compositor. "
				"If your compositor is running, check or set the "
				"WAYLAND_DISPLAY environment variable.");
		exit(LAB_EXIT_FAILURE);
	}

	nag->scale = 1;

	struct wl_registry *registry = wl_display_get_registry(nag->display);
	wl_registry_add_listener(registry, &registry_listener, nag);
	if (wl_display_roundtrip(nag->display) < 0) {
		wlr_log(WLR_ERROR, "failed to register with the wayland display");
		exit(LAB_EXIT_FAILURE);
	}

	assert(nag->compositor && nag->layer_shell && nag->shm);

	/* Second roundtrip to get wl_output properties */
	if (wl_display_roundtrip(nag->display) < 0) {
		wlr_log(WLR_ERROR, "Error during outputs init.");
		nag_destroy(nag);
		exit(LAB_EXIT_FAILURE);
	}

	if (!nag->output && nag->conf->output) {
		wlr_log(WLR_ERROR, "Output '%s' not found", nag->conf->output);
		nag_destroy(nag);
		exit(LAB_EXIT_FAILURE);
	}

	if (!nag->cursor_shape_manager) {
		nag_setup_cursors(nag);
	}

	nag->surface = wl_compositor_create_surface(nag->compositor);
	assert(nag->surface);
	wl_surface_add_listener(nag->surface, &surface_listener, nag);

	nag->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
			nag->layer_shell, nag->surface,
			nag->output ? nag->output->wl_output : NULL,
			nag->conf->layer,
			"nag");
	assert(nag->layer_surface);
	zwlr_layer_surface_v1_add_listener(nag->layer_surface,
			&layer_surface_listener, nag);
	zwlr_layer_surface_v1_set_anchor(nag->layer_surface,
			nag->conf->anchors);
	zwlr_layer_surface_v1_set_keyboard_interactivity(nag->layer_surface,
			nag->conf->keyboard_focus);

	wl_registry_destroy(registry);

	nag->pollfds[FD_WAYLAND].fd = wl_display_get_fd(nag->display);
	nag->pollfds[FD_WAYLAND].events = POLLIN;

	if (nag->details.close_timeout != 0) {
		nag->pollfds[FD_TIMER].fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
		nag->pollfds[FD_TIMER].events = POLLIN;
		struct itimerspec timeout = {
			.it_value.tv_sec = nag->details.close_timeout,
		};
		timerfd_settime(nag->pollfds[FD_TIMER].fd, 0, &timeout, NULL);
	} else {
		nag->pollfds[FD_TIMER].fd = -1;
	}

	sigset_t mask;
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigprocmask(SIG_BLOCK, &mask, NULL);
	nag->pollfds[FD_SIGNAL].fd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
	nag->pollfds[FD_SIGNAL].events = POLLIN;
}

static void
close_pollfd(struct pollfd *pollfd)
{
	if (pollfd->fd == -1) {
		return;
	}
	close(pollfd->fd);
	pollfd->fd = -1;
	pollfd->events = 0;
	pollfd->revents = 0;
}

static void
nag_run(struct nag *nag)
{
	nag->run_display = true;
	render_frame(nag);
	while (nag->run_display) {
		while (wl_display_prepare_read(nag->display) != 0) {
			wl_display_dispatch_pending(nag->display);
		}

		errno = 0;
		if (wl_display_flush(nag->display) == -1 && errno != EAGAIN) {
			break;
		}

		if (!nag->run_display) {
			break;
		}

		poll(nag->pollfds, NR_FDS, -1);
		if (nag->pollfds[FD_WAYLAND].revents & POLLIN) {
			wl_display_read_events(nag->display);
		} else {
			wl_display_cancel_read(nag->display);
		}
		if (nag->pollfds[FD_TIMER].revents & POLLIN) {
			exit_status = LAB_EXIT_CANCELLED;
			break;
		}
		if (nag->pollfds[FD_SIGNAL].revents & POLLIN) {
			break;
		}
	}
}

static void
conf_init(struct conf *conf)
{
	conf->font_description = pango_font_description_from_string("pango:Sans 10");
	conf->anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	conf->layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
	conf->keyboard_focus = ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE;
	conf->bar_border_thickness = 2;
	conf->message_padding = 8;
	conf->details_border_thickness = 3;
	conf->button_border_thickness = 3;
	conf->button_gap = 20;
	conf->button_gap_close = 15;
	conf->button_margin_right = 2;
	conf->button_padding = 3;
	conf->button_background = 0x680A0AFF;
	conf->details_background = 0x680A0AFF;
	conf->background = 0x900000FF;
	conf->text = 0xFFFFFFFF;
	conf->button_text = 0xFFFFFFFF;
	conf->button_border = 0xD92424FF;
	conf->border_bottom = 0x470909FF;
}

static bool
parse_color(const char *color, uint32_t *result)
{
	if (color[0] == '#') {
		++color;
	}
	int len = strlen(color);
	if ((len != 6 && len != 8) || !isxdigit(color[0]) || !isxdigit(color[1])) {
		return false;
	}
	char *ptr;
	uint32_t parsed = strtoul(color, &ptr, 16);
	if (*ptr != '\0') {
		return false;
	}
	*result = len == 6 ? ((parsed << 8) | 0xFF) : parsed;
	return true;
}

/*
 * As labnag is slow for large "detailed messages" we curtail stdin at an
 * arbitrary size to avoid hogging the CPU.
 */
#define MAX_STDIN_LINES 200
static char *
read_and_trim_stdin(void)
{
	char *buffer = NULL, *line = NULL;
	size_t buffer_len = 0, line_size = 0;
	int line_count = 0;
	while (line_count < MAX_STDIN_LINES) {
		ssize_t nread = getline(&line, &line_size, stdin);
		if (nread == -1) {
			if (feof(stdin)) {
				break;
			} else {
				perror("getline");
				goto freeline;
			}
		}
		buffer = realloc(buffer, buffer_len + nread + 1);
		if (!buffer) {
			perror("realloc");
			return NULL;
		}
		memcpy(&buffer[buffer_len], line, nread + 1);
		buffer_len += nread;
		++line_count;
	}
	free(line);

	while (buffer_len && buffer[buffer_len - 1] == '\n') {
		buffer[--buffer_len] = '\0';
	}

	return buffer;

freeline:
	free(line);
	return NULL;
}

static int
nag_parse_options(int argc, char **argv, struct nag *nag,
		struct conf *conf, bool *debug)
{
	enum type_options {
		TO_COLOR_BACKGROUND = 256,
		TO_COLOR_BUTTON_BORDER,
		TO_COLOR_BORDER_BOTTOM,
		TO_COLOR_BUTTON_BG,
		TO_COLOR_DETAILS,
		TO_COLOR_TEXT,
		TO_COLOR_BUTTON_TEXT,
		TO_THICK_BAR_BORDER,
		TO_PADDING_MESSAGE,
		TO_THICK_DET_BORDER,
		TO_THICK_BTN_BORDER,
		TO_GAP_BTN,
		TO_GAP_BTN_DISMISS,
		TO_MARGIN_BTN_RIGHT,
		TO_PADDING_BTN,
	};

	static const struct option opts[] = {
		{"button", required_argument, NULL, 'B'},
		{"button-dismiss", required_argument, NULL, 'Z'},
		{"debug", no_argument, NULL, 'd'},
		{"edge", required_argument, NULL, 'e'},
		{"layer", required_argument, NULL, 'y'},
		{"keyboard-focus", required_argument, NULL, 'k'},
		{"font", required_argument, NULL, 'f'},
		{"help", no_argument, NULL, 'h'},
		{"detailed-message", no_argument, NULL, 'l'},
		{"detailed-button", required_argument, NULL, 'L'},
		{"message", required_argument, NULL, 'm'},
		{"output", required_argument, NULL, 'o'},
		{"timeout", required_argument, NULL, 't'},
		{"version", no_argument, NULL, 'v'},

		{"background-color", required_argument, NULL, TO_COLOR_BACKGROUND},
		{"button-border-color", required_argument, NULL, TO_COLOR_BUTTON_BORDER},
		{"border-bottom-color", required_argument, NULL, TO_COLOR_BORDER_BOTTOM},
		{"button-background-color", required_argument, NULL, TO_COLOR_BUTTON_BG},
		{"text-color", required_argument, NULL, TO_COLOR_TEXT},
		{"button-text-color", required_argument, NULL, TO_COLOR_BUTTON_TEXT},
		{"border-bottom-size", required_argument, NULL, TO_THICK_BAR_BORDER},
		{"message-padding", required_argument, NULL, TO_PADDING_MESSAGE},
		{"details-border-size", required_argument, NULL, TO_THICK_DET_BORDER},
		{"details-background-color", required_argument, NULL, TO_COLOR_DETAILS},
		{"button-border-size", required_argument, NULL, TO_THICK_BTN_BORDER},
		{"button-gap", required_argument, NULL, TO_GAP_BTN},
		{"button-dismiss-gap", required_argument, NULL, TO_GAP_BTN_DISMISS},
		{"button-margin-right", required_argument, NULL, TO_MARGIN_BTN_RIGHT},
		{"button-padding", required_argument, NULL, TO_PADDING_BTN},

		{0, 0, 0, 0}
	};

	const char *usage =
		"Usage: labnag [options...]\n"
		"\n"
		"  -B, --button <text> [<action>]  Create a button with text\n"
		"  -Z, --button-dismiss <text> [<action>]\n"
		"                                  Like -B but dismiss nag when pressed\n"
		"  -d, --debug                     Enable debugging.\n"
		"  -e, --edge top|bottom           Set the edge to use.\n"
		"  -y, --layer overlay|top|bottom|background\n"
		"                                  Set the layer to use.\n"
		"  -k, --keyboard-focus none|exclusive|on-demand|\n"
		"                                  Set the policy for keyboard focus.\n"
		"  -f, --font <font>               Set the font to use.\n"
		"  -h, --help                      Show help message and quit.\n"
		"  -l, --detailed-message          Read a detailed message from stdin.\n"
		"  -L, --detailed-button <text>    Set the text of the detail button.\n"
		"  -m, --message <msg>             Set the message text.\n"
		"  -o, --output <output>           Set the output to use.\n"
		"  -t, --timeout <seconds>         Set duration to close dialog.\n"
		"  -x, --exclusive-zone            Use exclusive zone.\n"
		"  -v, --version                   Show the version number and quit.\n"
		"\n"
		"The following appearance options can also be given:\n"
		"  --background-color RRGGBB[AA]    Background color.\n"
		"  --button-border-color RRGGBB[AA] Button border color.\n"
		"  --border-bottom-color RRGGBB[AA] Bottom border color.\n"
		"  --button-background-color RRGGBB[AA]\n"
		"                                   Button background color.\n"
		"  --text-color RRGGBB[AA]          Text color.\n"
		"  --button-text-color RRGGBB[AA]   Button text color.\n"
		"  --border-bottom-size size        Thickness of the bar border.\n"
		"  --message-padding padding        Padding for the message.\n"
		"  --details-border-size size       Thickness for the details border.\n"
		"  --details-background-color RRGGBB[AA]\n"
		"                                   Details background color.\n"
		"  --button-border-size size        Thickness for the button border.\n"
		"  --button-gap gap                 Size of the gap between buttons\n"
		"  --button-dismiss-gap gap         Size of the gap for dismiss button.\n"
		"  --button-margin-right margin     Margin from dismiss button to edge.\n"
		"  --button-padding padding         Padding for the button text.\n";

	optind = 1;
	while (1) {
		int c = getopt_long(argc, argv, "B:Z:c:de:y:k:f:hlL:m:o:s:t:vx", opts, NULL);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'B': /* Button */
		case 'Z': /* Button (Dismiss) */ {
			struct button *button = calloc(1, sizeof(*button));
			if (!button) {
				perror("calloc");
				return LAB_EXIT_FAILURE;
			}
			if (argv[optind] && argv[optind][0] != '-') {
				button->action = argv[optind];
				optind++;
			}
			button->text = optarg;
			button->dismiss = c == 'Z';
			wl_list_insert(&nag->buttons, &button->link);
			break;
		}
		case 'd': /* Debug */
			*debug = true;
			break;
		case 'e': /* Edge */
			if (strcmp(optarg, "top") == 0) {
				conf->anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
					| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
					| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
			} else if (strcmp(optarg, "bottom") == 0) {
				conf->anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
					| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
					| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
			} else {
				fprintf(stderr, "Invalid edge: %s\n", optarg);
				return LAB_EXIT_FAILURE;
			}
			break;
		case 'y': /* Layer */
			if (strcmp(optarg, "background") == 0) {
				conf->layer = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
			} else if (strcmp(optarg, "bottom") == 0) {
				conf->layer = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
			} else if (strcmp(optarg, "top") == 0) {
				conf->layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
			} else if (strcmp(optarg, "overlay") == 0) {
				conf->layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
			} else {
				fprintf(stderr, "Invalid layer: %s\n"
						"Usage: --layer overlay|top|bottom|background\n",
						optarg);
				return LAB_EXIT_FAILURE;
			}
			break;
		case 'k':
			if (strcmp(optarg, "none") == 0) {
				conf->keyboard_focus =
					ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE;
			} else if (strcmp(optarg, "exclusive") == 0) {
				conf->keyboard_focus =
					ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE;
			} else if (strcmp(optarg, "on-demand") == 0) {
				conf->keyboard_focus =
					ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND;
			} else {
				fprintf(stderr, "Invalid keyboard focus: %s\n"
						"Usage: --keyboard-focus none|exclusive|on-demand\n",
						optarg);
				return LAB_EXIT_FAILURE;
			}
			break;
		case 'f': /* Font */
			pango_font_description_free(conf->font_description);
			conf->font_description = pango_font_description_from_string(optarg);
			break;
		case 'l': /* Detailed Message */
			free(nag->details.message);
			nag->details.message = read_and_trim_stdin();
			if (!nag->details.message) {
				return LAB_EXIT_FAILURE;
			}
			nag->details.button_up.text = "▲";
			nag->details.button_down.text = "▼";
			break;
		case 'L': /* Detailed Button Text */
			nag->details.details_text = optarg;
			break;
		case 'm': /* Message */
			nag->message = optarg;
			break;
		case 'o': /* Output */
			free(conf->output);
			conf->output = optarg;
			break;
		case 't':
			nag->details.close_timeout = atoi(optarg);
			break;
		case 'x':
			nag->details.use_exclusive_zone = true;
			break;
		case 'v': /* Version */
			printf("labnag " LABWC_VERSION "\n");
			return LAB_EXIT_FAILURE;
		case TO_COLOR_BACKGROUND: /* Background color */
			if (!parse_color(optarg, &conf->background)) {
				fprintf(stderr, "Invalid background color: %s\n", optarg);
			}
			break;
		case TO_COLOR_BUTTON_BORDER: /* Border color */
			if (!parse_color(optarg, &conf->button_border)) {
				fprintf(stderr, "Invalid border color: %s\n", optarg);
			}
			break;
		case TO_COLOR_BORDER_BOTTOM: /* Bottom border color */
			if (!parse_color(optarg, &conf->border_bottom)) {
				fprintf(stderr, "Invalid border bottom color: %s\n", optarg);
			}
			break;
		case TO_COLOR_BUTTON_BG: /* Button background color */
			if (!parse_color(optarg, &conf->button_background)) {
				fprintf(stderr, "Invalid button background color: %s\n", optarg);
			}
			break;
		case TO_COLOR_DETAILS: /* Details background color */
			if (!parse_color(optarg, &conf->details_background)) {
				fprintf(stderr, "Invalid details background color: %s\n", optarg);
			}
			break;
		case TO_COLOR_TEXT: /* Text color */
			if (!parse_color(optarg, &conf->text)) {
				fprintf(stderr, "Invalid text color: %s\n", optarg);
			}
			break;
		case TO_COLOR_BUTTON_TEXT: /* Button text color */
			if (!parse_color(optarg, &conf->button_text)) {
				fprintf(stderr, "Invalid button text color: %s\n", optarg);
			}
			break;
		case TO_THICK_BAR_BORDER: /* Bottom border thickness */
			conf->bar_border_thickness = strtol(optarg, NULL, 0);
			break;
		case TO_PADDING_MESSAGE: /* Message padding */
			conf->message_padding = strtol(optarg, NULL, 0);
			break;
		case TO_THICK_DET_BORDER: /* Details border thickness */
			conf->details_border_thickness = strtol(optarg, NULL, 0);
			break;
		case TO_THICK_BTN_BORDER: /* Button border thickness */
			conf->button_border_thickness = strtol(optarg, NULL, 0);
			break;
		case TO_GAP_BTN: /* Gap between buttons */
			conf->button_gap = strtol(optarg, NULL, 0);
			break;
		case TO_GAP_BTN_DISMISS: /* Gap between dismiss button */
			conf->button_gap_close = strtol(optarg, NULL, 0);
			break;
		case TO_MARGIN_BTN_RIGHT: /* Margin on the right side of button area */
			conf->button_margin_right = strtol(optarg, NULL, 0);
			break;
		case TO_PADDING_BTN: /* Padding for the button text */
			conf->button_padding = strtol(optarg, NULL, 0);
			break;
		default: /* Help or unknown flag */
			fprintf(c == 'h' ? stdout : stderr, "%s", usage);
			return LAB_EXIT_FAILURE;
		}
	}

	return LAB_EXIT_SUCCESS;
}

int
main(int argc, char **argv)
{
	struct conf conf = { 0 };
	conf_init(&conf);

	struct nag nag = {
		.conf = &conf,
	};

	wl_list_init(&nag.buttons);
	wl_list_init(&nag.outputs);
	wl_list_init(&nag.seats);

	nag.details.details_text = "Toggle details";
	nag.details.close_timeout = 5;
	nag.details.use_exclusive_zone = false;

	bool debug = false;
	if (argc > 1) {
		exit_status = nag_parse_options(argc, argv, &nag, &conf, &debug);
		if (exit_status == LAB_EXIT_FAILURE) {
			goto cleanup;
		}
	}
	wlr_log_init(debug ? WLR_DEBUG : WLR_ERROR, NULL);

	if (!nag.message) {
		wlr_log(WLR_ERROR, "No message passed. Please provide --message/-m");
		exit_status = LAB_EXIT_FAILURE;
		goto cleanup;
	}

	if (nag.details.message) {
		nag.details.button_details = calloc(1, sizeof(struct button));
		assert(nag.details.button_details);
		nag.details.button_details->text = nag.details.details_text;
		assert(nag.details.button_details->text);
		nag.details.button_details->expand = true;
		wl_list_insert(nag.buttons.prev, &nag.details.button_details->link);
	}

	int nr_buttons = wl_list_length(&nag.buttons);
	if (conf.keyboard_focus && nr_buttons > 0) {
		/* select the leftmost button */
		nag.selected_button = nr_buttons - 1;
	} else {
		nag.selected_button = -1;
	}

	wlr_log(WLR_DEBUG, "Output: %s", nag.conf->output);
	wlr_log(WLR_DEBUG, "Anchors: %lu", (unsigned long)nag.conf->anchors);
	wlr_log(WLR_DEBUG, "Message: %s", nag.message);
	char *font = pango_font_description_to_string(nag.conf->font_description);
	wlr_log(WLR_DEBUG, "Font: %s", font);
	free(font);
	wlr_log(WLR_DEBUG, "Buttons");

	struct button *button;
	wl_list_for_each(button, &nag.buttons, link) {
		wlr_log(WLR_DEBUG, "\t[%s] `%s`", button->text, button->action);
	}

	nag_setup(&nag);

	nag_run(&nag);

cleanup:
	nag_destroy(&nag);
	return exit_status;
}
