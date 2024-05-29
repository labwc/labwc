// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include "common/mem.h"
#include "labwc.h"
#include "node.h"

struct session_lock_output {
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *background;
	struct session_lock_manager *manager;
	struct output *output;
	struct wlr_session_lock_surface_v1 *surface;
	struct wl_event_source *blank_timer;

	struct wl_list link; /* session_lock_manager.outputs */

	struct wl_listener destroy;
	struct wl_listener commit;
	struct wl_listener surface_destroy;
	struct wl_listener surface_map;
};

static void
focus_surface(struct session_lock_manager *manager, struct wlr_surface *focused)
{
	manager->focused = focused;
	seat_focus_lock_surface(&manager->server->seat, focused);
}

static void
refocus_output(struct session_lock_output *output)
{
	/* Try to focus another session-lock surface */
	if (output->manager->focused != output->surface->surface) {
		return;
	}

	struct session_lock_output *iter;
	wl_list_for_each(iter, &output->manager->session_lock_outputs, link) {
		if (iter == output || !iter->surface || !iter->surface->surface) {
			continue;
		}
		if (iter->surface->surface->mapped) {
			focus_surface(output->manager, iter->surface->surface);
			return;
		}
	}
	focus_surface(output->manager, NULL);
}

static void
update_focus(void *data)
{
	struct session_lock_output *output = data;
	cursor_update_focus(output->manager->server);
	if (!output->manager->focused) {
		focus_surface(output->manager, output->surface->surface);
	}
}

static void
handle_surface_map(struct wl_listener *listener, void *data)
{
	struct session_lock_output *output =
		wl_container_of(listener, output, surface_map);
	/*
	 * In order to update cursor shape on map, the call to
	 * cursor_update_focus() should be delayed because only the
	 * role-specific surface commit/map handler has been processed in
	 * wlroots at this moment and get_cursor_context() returns NULL as a
	 * buffer has not been actually attached to the surface.
	 */
	wl_event_loop_add_idle(
		output->manager->server->wl_event_loop, update_focus, output);
}

static void
handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct session_lock_output *output =
		wl_container_of(listener, output, surface_destroy);
	refocus_output(output);

	assert(output->surface);
	output->surface = NULL;
	wl_list_remove(&output->surface_destroy.link);
	wl_list_remove(&output->surface_map.link);
}

static void
lock_output_reconfigure(struct session_lock_output *output)
{
	struct wlr_box box;
	wlr_output_layout_get_box(output->manager->server->output_layout,
		output->output->wlr_output, &box);
	wlr_scene_rect_set_size(output->background, box.width, box.height);
	if (output->surface) {
		wlr_session_lock_surface_v1_configure(
			output->surface, box.width, box.height);
	}
}

static void
handle_new_surface(struct wl_listener *listener, void *data)
{
	struct session_lock_manager *manager =
		wl_container_of(listener, manager, lock_new_surface);
	struct wlr_session_lock_surface_v1 *lock_surface = data;
	struct output *output = lock_surface->output->data;
	struct session_lock_output *lock_output;
	wl_list_for_each(lock_output, &manager->session_lock_outputs, link) {
		if (lock_output->output == output) {
			goto found_lock_output;
		}
	}
	wlr_log(WLR_ERROR, "new lock surface, but no output");
	/* TODO: Consider improving security by handling this better */
	return;

found_lock_output:
	lock_output->surface = lock_surface;
	struct wlr_scene_tree *surface_tree =
		wlr_scene_subsurface_tree_create(lock_output->tree, lock_surface->surface);
	node_descriptor_create(&surface_tree->node,
		LAB_NODE_DESC_SESSION_LOCK_SURFACE, NULL);

	lock_output->surface_destroy.notify = handle_surface_destroy;
	wl_signal_add(&lock_surface->events.destroy, &lock_output->surface_destroy);

	lock_output->surface_map.notify = handle_surface_map;
	wl_signal_add(&lock_surface->surface->events.map, &lock_output->surface_map);

	lock_output_reconfigure(lock_output);
}

static void
session_lock_output_destroy(struct session_lock_output *output)
{
	if (output->surface) {
		refocus_output(output);
		wl_list_remove(&output->surface_destroy.link);
		wl_list_remove(&output->surface_map.link);
	}
	wl_list_remove(&output->commit.link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->link);
	wl_event_source_remove(output->blank_timer);
	free(output);
}

static void
handle_output_destroy(struct wl_listener *listener, void *data)
{
	struct session_lock_output *output = wl_container_of(listener, output, destroy);
	session_lock_output_destroy(output);
}

static void
handle_output_commit(struct wl_listener *listener, void *data)
{
	struct wlr_output_event_commit *event = data;
	struct session_lock_output *output = wl_container_of(listener, output, commit);
	uint32_t require_reconfigure = WLR_OUTPUT_STATE_MODE
		| WLR_OUTPUT_STATE_SCALE | WLR_OUTPUT_STATE_TRANSFORM;
	if (event->state->committed & require_reconfigure) {
		lock_output_reconfigure(output);
	}
}

static void
align_session_lock_tree(struct output *output)
{
	struct wlr_box box;
	wlr_output_layout_get_box(output->server->output_layout,
		output->wlr_output, &box);
	wlr_scene_node_set_position(&output->session_lock_tree->node, box.x, box.y);
}

static int
handle_output_blank_timeout(void *data)
{
	struct session_lock_output *lock_output = data;
	wlr_scene_node_set_enabled(&lock_output->background->node, true);
	return 0;
}

void
session_lock_output_create(struct session_lock_manager *manager, struct output *output)
{
	struct session_lock_output *lock_output = znew(*lock_output);

	struct wlr_scene_tree *tree = wlr_scene_tree_create(output->session_lock_tree);
	if (!tree) {
		wlr_log(WLR_ERROR, "session-lock: wlr_scene_tree_create()");
		free(lock_output);
		goto exit_session;
	}

	/*
	 * The ext-session-lock protocol says that the compositor should blank
	 * all outputs with an opaque color such that their normal content is
	 * fully hidden
	 */
	float *black = (float[4]) { 0.f, 0.f, 0.f, 1.f };
	struct wlr_scene_rect *background = wlr_scene_rect_create(tree, 0, 0, black);
	if (!background) {
		wlr_log(WLR_ERROR, "session-lock: wlr_scene_rect_create()");
		wlr_scene_node_destroy(&tree->node);
		free(lock_output);
		goto exit_session;
	}

	/* Delay blanking output by 100ms to prevent flashing */
	lock_output->blank_timer =
		wl_event_loop_add_timer(manager->server->wl_event_loop,
			handle_output_blank_timeout, lock_output);
	if (!manager->locked) {
		wlr_scene_node_set_enabled(&background->node, false);
		wl_event_source_timer_update(lock_output->blank_timer, 100);
	}

	align_session_lock_tree(output);

	lock_output->output = output;
	lock_output->tree = tree;
	lock_output->background = background;
	lock_output->manager = manager;

	lock_output->destroy.notify = handle_output_destroy;
	wl_signal_add(&tree->node.events.destroy, &lock_output->destroy);

	lock_output->commit.notify = handle_output_commit;
	wl_signal_add(&output->wlr_output->events.commit, &lock_output->commit);

	lock_output_reconfigure(lock_output);

	wl_list_insert(&manager->session_lock_outputs, &lock_output->link);
	return;

exit_session:
	/* TODO: Consider a better - but secure - way to deal with this */
	wlr_log(WLR_ERROR, "out of memory");
	exit(EXIT_FAILURE);
}

static void
session_lock_destroy(struct session_lock_manager *manager)
{
	struct session_lock_output *lock_output, *next;
	wl_list_for_each_safe(lock_output, next, &manager->session_lock_outputs, link) {
		wlr_scene_node_destroy(&lock_output->tree->node);
	}
	if (manager->lock) {
		wl_list_remove(&manager->lock_destroy.link);
		wl_list_remove(&manager->lock_unlock.link);
		wl_list_remove(&manager->lock_new_surface.link);
	}
	manager->lock = NULL;
}

static void
handle_lock_unlock(struct wl_listener *listener, void *data)
{
	struct session_lock_manager *manager =
		wl_container_of(listener, manager, lock_unlock);
	session_lock_destroy(manager);
	manager->locked = false;
	desktop_focus_topmost_view(manager->server);
	cursor_update_focus(manager->server);
}

static void
handle_lock_destroy(struct wl_listener *listener, void *data)
{
	struct session_lock_manager *manager =
		wl_container_of(listener, manager, lock_destroy);

	float *black = (float[4]) { 0.f, 0.f, 0.f, 1.f };
	struct session_lock_output *lock_output;
	wl_list_for_each(lock_output, &manager->session_lock_outputs, link) {
		wlr_scene_rect_set_color(lock_output->background, black);
	}

	wl_list_remove(&manager->lock_destroy.link);
	wl_list_remove(&manager->lock_unlock.link);
	wl_list_remove(&manager->lock_new_surface.link);
}

static void
handle_new_session_lock(struct wl_listener *listener, void *data)
{
	struct session_lock_manager *manager =
		wl_container_of(listener, manager, new_lock);
	struct wlr_session_lock_v1 *lock = data;

	if (manager->lock) {
		wlr_log(WLR_ERROR, "session already locked");
		wlr_session_lock_v1_destroy(lock);
		return;
	}
	if (manager->locked) {
		wlr_log(WLR_INFO, "replacing abandoned lock");
		session_lock_destroy(manager);
	}
	assert(wl_list_empty(&manager->session_lock_outputs));

	struct output *output;
	wl_list_for_each(output, &manager->server->outputs, link) {
		session_lock_output_create(manager, output);
	}

	manager->lock_new_surface.notify = handle_new_surface;
	wl_signal_add(&lock->events.new_surface, &manager->lock_new_surface);

	manager->lock_unlock.notify = handle_lock_unlock;
	wl_signal_add(&lock->events.unlock, &manager->lock_unlock);

	manager->lock_destroy.notify = handle_lock_destroy;
	wl_signal_add(&lock->events.destroy, &manager->lock_destroy);

	manager->locked = true;
	wlr_session_lock_v1_send_locked(lock);
}

static void
handle_manager_destroy(struct wl_listener *listener, void *data)
{
	struct session_lock_manager *manager =
		wl_container_of(listener, manager, destroy);
	session_lock_destroy(manager);
	wl_list_remove(&manager->new_lock.link);
	wl_list_remove(&manager->destroy.link);
	manager->server->session_lock_manager = NULL;
	free(manager);
}

void
session_lock_init(struct server *server)
{
	struct session_lock_manager *manager = znew(*manager);
	server->session_lock_manager = manager;
	manager->server = server;
	manager->wlr_manager = wlr_session_lock_manager_v1_create(server->wl_display);
	wl_list_init(&manager->session_lock_outputs);

	manager->new_lock.notify = handle_new_session_lock;
	wl_signal_add(&manager->wlr_manager->events.new_lock, &manager->new_lock);

	manager->destroy.notify = handle_manager_destroy;
	wl_signal_add(&manager->wlr_manager->events.destroy, &manager->destroy);
}

void
session_lock_update_for_layout_change(struct server *server)
{
	if (!server->session_lock_manager->locked) {
		return;
	}

	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		align_session_lock_tree(output);
	}

	struct session_lock_manager *manager = server->session_lock_manager;
	struct session_lock_output *lock_output;
	wl_list_for_each(lock_output, &manager->session_lock_outputs, link) {
		lock_output_reconfigure(lock_output);
	}
}
