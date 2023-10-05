// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <stdlib.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include "common/mem.h"
#include "idle.h"

struct lab_idle_inhibitor {
	struct wlr_idle_inhibitor_v1 *wlr_inhibitor;
	struct wl_listener on_destroy;
};

struct lab_idle_manager {
	struct wlr_idle_notifier_v1 *ext;
	struct {
		struct wlr_idle_inhibit_manager_v1 *manager;
		struct wl_listener on_new_inhibitor;
	} inhibitor;
	struct wlr_seat *wlr_seat;
	struct wl_listener on_display_destroy;
};

static struct lab_idle_manager *manager;

static void
handle_idle_inhibitor_destroy(struct wl_listener *listener, void *data)
{
	struct lab_idle_inhibitor *idle_inhibitor = wl_container_of(listener,
		idle_inhibitor, on_destroy);

	if (manager) {
		/*
		 * The display destroy event might have been triggered
		 * already and thus the manager would be NULL.
		 */
		bool still_inhibited =
			wl_list_length(&manager->inhibitor.manager->inhibitors) > 1;
		wlr_idle_notifier_v1_set_inhibited(manager->ext, still_inhibited);
	}

	wl_list_remove(&idle_inhibitor->on_destroy.link);
	free(idle_inhibitor);
}

static void
handle_idle_inhibitor_new(struct wl_listener *listener, void *data)
{
	assert(manager);
	struct wlr_idle_inhibitor_v1 *wlr_inhibitor = data;

	struct lab_idle_inhibitor *inhibitor = znew(*inhibitor);
	inhibitor->wlr_inhibitor = wlr_inhibitor;
	inhibitor->on_destroy.notify = handle_idle_inhibitor_destroy;
	wl_signal_add(&wlr_inhibitor->events.destroy, &inhibitor->on_destroy);

	wlr_idle_notifier_v1_set_inhibited(manager->ext, true);
}

static void
handle_display_destroy(struct wl_listener *listener, void *data)
{
	/*
	 * All the managers will react to the display
	 * destroy signal as well and thus clean up.
	 */
	wl_list_remove(&manager->on_display_destroy.link);
	zfree(manager);
}

void
idle_manager_create(struct wl_display *display, struct wlr_seat *wlr_seat)
{
	assert(!manager);
	manager = znew(*manager);
	manager->wlr_seat = wlr_seat;

	manager->ext = wlr_idle_notifier_v1_create(display);

	manager->inhibitor.manager = wlr_idle_inhibit_v1_create(display);
	manager->inhibitor.on_new_inhibitor.notify = handle_idle_inhibitor_new;
	wl_signal_add(&manager->inhibitor.manager->events.new_inhibitor,
		&manager->inhibitor.on_new_inhibitor);

	manager->on_display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->on_display_destroy);
}

void
idle_manager_notify_activity(struct wlr_seat *seat)
{
	/*
	 * The display destroy event might have been triggered
	 * already and thus the manager would be NULL. Due to
	 * future code changes we might also get called before
	 * the manager has been created.
	 */
	if (!manager) {
		return;
	}

	wlr_idle_notifier_v1_notify_activity(manager->ext, seat);
}
