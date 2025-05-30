/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SCALED_ICON_BUFFER_H
#define LABWC_SCALED_ICON_BUFFER_H

#include <stdbool.h>
#include <wayland-server-core.h>

struct wlr_scene_tree;
struct wlr_scene_node;
struct wlr_scene_buffer;

struct scaled_icon_buffer {
	struct scaled_scene_buffer *scaled_buffer;
	struct wlr_scene_buffer *scene_buffer;
	struct server *server;
	/* for window icon */
	struct view *view;
	char *view_app_id;
	char *view_icon_name;
	struct wl_array view_icon_buffers;
	struct {
		struct wl_listener set_icon;
		struct wl_listener destroy;
	} on_view;
	/* for general icon (e.g. in menus) */
	char *icon_name;

	int width;
	int height;
};

/*
 * Create an auto scaling icon buffer, providing a wlr_scene_buffer node for
 * display. It gets destroyed automatically when the backing scaled_scene_buffer
 * is being destroyed which in turn happens automatically when the backing
 * wlr_scene_buffer (or one of its parents) is being destroyed.
 */
struct scaled_icon_buffer *scaled_icon_buffer_create(
	struct wlr_scene_tree *parent, struct server *server,
	int width, int height);

void scaled_icon_buffer_set_view(struct scaled_icon_buffer *self,
	struct view *view);

void scaled_icon_buffer_set_icon_name(struct scaled_icon_buffer *self,
	const char *icon_name);

/* Obtain scaled_icon_buffer from wlr_scene_node */
struct scaled_icon_buffer *scaled_icon_buffer_from_node(struct wlr_scene_node *node);

#endif /* LABWC_SCALED_ICON_BUFFER_H */
