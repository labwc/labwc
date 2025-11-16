/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_DESKTOP_STATE_H
#define LABWC_DESKTOP_STATE_H
#include <wayland-util.h>

struct server;
struct output;

struct desktop_output {
	struct output *output;
	double scale;
	struct wl_array views;
	struct wl_list link;
};

/* Helpers to remember windows and scales for each output */
void desktop_state_create(struct server *server, struct wl_list *desktop_outputs);
void desktop_state_destroy(struct wl_list *desktop_outputs);

/**
 * desktop_state_adjust_on_output_scale_change() - adjust views on scale change
 * @server: server
 * @desktop_outputs: object list containing scale and array of views
 *
 * Put windows back on the pre-scale-change output and also adjust position/size
 * to fit the usable area.
 */
void desktop_state_adjust_on_output_scale_change(struct server *server,
	struct wl_list *desktop_outputs);

#endif /* LABWC_DESKTOP_STATE_H */
