// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include "common/mem.h"
#include "labwc.h"

static struct wl_listener new_lock;
static struct wl_listener manager_destroy;
static struct wlr_session_lock_manager_v1 *wlr_session_lock_manager;

static struct server *g_server;

struct session_lock_output {
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *background;
	struct session_lock *lock;
	struct output *output;
	struct wlr_session_lock_surface_v1 *surface;

	struct wl_list link; /* session_lock.outputs */

	struct wl_listener destroy;
	struct wl_listener commit;
	struct wl_listener surface_destroy;
	struct wl_listener surface_map;
};

static void
focus_surface(struct session_lock *lock, struct wlr_surface *focused)
{
	lock->focused = focused;
	seat_focus_surface(&g_server->seat, focused);
}

static void
refocus_output(struct session_lock_output *output)
{
	/* Try to focus another session-lock surface */
	if (output->lock->focused != output->surface->surface) {
		return;
	}

	struct session_lock_output *iter;
	wl_list_for_each(iter, &output->lock->session_lock_outputs, link) {
		if (iter == output || !iter->surface) {
			continue;
		}
		if (iter->surface->mapped) {
			focus_surface(output->lock, iter->surface->surface);
			return;
		}
	}
	focus_surface(output->lock, NULL);
}

static void
handle_surface_map(struct wl_listener *listener, void *data)
{
	struct session_lock_output *surf = wl_container_of(listener, surf, surface_map);
	if (!surf->lock->focused) {
		focus_surface(surf->lock, surf->surface->surface);
	}
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
	wlr_output_layout_get_box(g_server->output_layout, output->output->wlr_output, &box);
	wlr_scene_rect_set_size(output->background, box.width, box.height);
	if (output->surface) {
		wlr_session_lock_surface_v1_configure(output->surface, box.width, box.height);
	}
}

static void
handle_new_surface(struct wl_listener *listener, void *data)
{
	struct session_lock *lock = wl_container_of(listener, lock, new_surface);
	struct wlr_session_lock_surface_v1 *lock_surface = data;
	struct output *output = lock_surface->output->data;
	struct session_lock_output *lock_output;
	wl_list_for_each(lock_output, &lock->session_lock_outputs, link) {
		if (lock_output->output == output) {
			goto found_lock_output;
		}
	}
	wlr_log(WLR_ERROR, "new lock surface, but no output");
	/* TODO: Consider improving security by handling this better */
	return;

found_lock_output:
	lock_output->surface = lock_surface;
	wlr_scene_subsurface_tree_create(lock_output->tree, lock_surface->surface);

	lock_output->surface_destroy.notify = handle_surface_destroy;
	wl_signal_add(&lock_surface->events.destroy, &lock_output->surface_destroy);

	lock_output->surface_map.notify = handle_surface_map;
	wl_signal_add(&lock_surface->events.map, &lock_output->surface_map);

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
	free(output);
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct session_lock_output *output = wl_container_of(listener, output, destroy);
	session_lock_output_destroy(output);
}

static void
handle_commit(struct wl_listener *listener, void *data)
{
	struct wlr_output_event_commit *event = data;
	struct session_lock_output *output = wl_container_of(listener, output, commit);
	uint32_t require_reconfigure = WLR_OUTPUT_STATE_MODE
		| WLR_OUTPUT_STATE_SCALE | WLR_OUTPUT_STATE_TRANSFORM;
	if (event->committed & require_reconfigure) {
		lock_output_reconfigure(output);
	}
}

static void
align_session_lock_tree(struct output *output)
{
	struct wlr_box box;
	wlr_output_layout_get_box(g_server->output_layout, output->wlr_output, &box);
	wlr_scene_node_set_position(&output->session_lock_tree->node, box.x, box.y);
}

void
session_lock_output_create(struct session_lock *lock, struct output *output)
{
	struct session_lock_output *lock_output = znew(*lock_output);
	if (!lock_output) {
		wlr_log(WLR_ERROR, "session-lock: out of memory");
		goto exit_session;
	}

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

	align_session_lock_tree(output);

	lock_output->output = output;
	lock_output->tree = tree;
	lock_output->background = background;
	lock_output->lock = lock;

	lock_output->destroy.notify = handle_destroy;
	wl_signal_add(&tree->node.events.destroy, &lock_output->destroy);

	lock_output->commit.notify = handle_commit;
	wl_signal_add(&output->wlr_output->events.commit, &lock_output->commit);

	lock_output_reconfigure(lock_output);

	wl_list_insert(&lock->session_lock_outputs, &lock_output->link);
	return;

exit_session:
	/* TODO: Consider a better - but secure - way to deal with this */
	wlr_log(WLR_ERROR, "out of memory");
	exit(EXIT_FAILURE);
}

static void
session_lock_destroy(struct session_lock *lock)
{
	struct session_lock_output *lock_output, *next;
	wl_list_for_each_safe(lock_output, next, &lock->session_lock_outputs, link) {
		wlr_scene_node_destroy(&lock_output->tree->node);
	}
	if (g_server->session_lock == lock) {
		g_server->session_lock = NULL;
	}
	if (!lock->abandoned) {
		wl_list_remove(&lock->destroy.link);
		wl_list_remove(&lock->unlock.link);
		wl_list_remove(&lock->new_surface.link);
	}
	free(lock);
}

static void
handle_unlock(struct wl_listener *listener, void *data)
{
	struct session_lock *lock = wl_container_of(listener, lock, unlock);
	session_lock_destroy(lock);
	desktop_focus_topmost_view(g_server);
}

static void
handle_session_lock_destroy(struct wl_listener *listener, void *data)
{
	struct session_lock *lock = wl_container_of(listener, lock, destroy);

	float *black = (float[4]) { 0.f, 0.f, 0.f, 1.f };
	struct session_lock_output *lock_output;
	wl_list_for_each(lock_output, &lock->session_lock_outputs, link) {
		wlr_scene_rect_set_color(lock_output->background, black);
	}

	lock->abandoned = true;
	wl_list_remove(&lock->destroy.link);
	wl_list_remove(&lock->unlock.link);
	wl_list_remove(&lock->new_surface.link);
}

static void
handle_new_session_lock(struct wl_listener *listener, void *data)
{
	struct wlr_session_lock_v1 *lock = data;

	/* One already exists */
	if (g_server->session_lock) {
		if (g_server->session_lock->abandoned) {
			wlr_log(WLR_INFO, "replacing abandoned lock");
			session_lock_destroy(g_server->session_lock);
		} else {
			wlr_log(WLR_ERROR, "session already locked");
			wlr_session_lock_v1_destroy(lock);
			return;
		}
	}

	struct session_lock *session_lock = znew(*session_lock);
	if (!session_lock) {
		wlr_log(WLR_ERROR, "session-lock: out of memory");
		wlr_session_lock_v1_destroy(lock);
		return;
	}

	wl_list_init(&session_lock->session_lock_outputs);
	struct output *output;
	wl_list_for_each(output, &g_server->outputs, link) {
		session_lock_output_create(session_lock, output);
	}

	session_lock->new_surface.notify = handle_new_surface;
	wl_signal_add(&lock->events.new_surface, &session_lock->new_surface);

	session_lock->unlock.notify = handle_unlock;
	wl_signal_add(&lock->events.unlock, &session_lock->unlock);

	session_lock->destroy.notify = handle_session_lock_destroy;
	wl_signal_add(&lock->events.destroy, &session_lock->destroy);

	wlr_session_lock_v1_send_locked(lock);
	g_server->session_lock = session_lock;
}

static void
handle_manager_destroy(struct wl_listener *listener, void *data)
{
	if (g_server->session_lock) {
		session_lock_destroy(g_server->session_lock);
	}
	wl_list_remove(&new_lock.link);
	wl_list_remove(&manager_destroy.link);
	wlr_session_lock_manager = NULL;
}

void
session_lock_init(struct server *server)
{
	g_server = server;
	wlr_session_lock_manager = wlr_session_lock_manager_v1_create(server->wl_display);

	new_lock.notify = handle_new_session_lock;
	wl_signal_add(&wlr_session_lock_manager->events.new_lock, &new_lock);

	manager_destroy.notify = handle_manager_destroy;
	wl_signal_add(&wlr_session_lock_manager->events.destroy, &manager_destroy);
}

void
session_lock_update_for_layout_change(void)
{
	if (!g_server->session_lock) {
		return;
	}

	struct output *output;
	wl_list_for_each(output, &g_server->outputs, link) {
		align_session_lock_tree(output);
	}

	struct session_lock *lock = g_server->session_lock;
	struct session_lock_output *lock_output;
	wl_list_for_each(lock_output, &lock->session_lock_outputs, link) {
		lock_output_reconfigure(lock_output);
	}
}
