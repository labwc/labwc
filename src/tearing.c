// SPDX-License-Identifier: GPL-2.0-only

#include "common/mem.h"
#include "labwc.h"
#include "view.h"

struct tearing_controller {
		struct wlr_tearing_control_v1 *tearing_control;
		struct wl_listener set_hint;
		struct wl_listener destroy;
};

static void
set_tearing_hint(struct wl_listener *listener, void *data)
{
	struct tearing_controller *controller = wl_container_of(listener, controller, set_hint);
	struct view *view = view_from_wlr_surface(controller->tearing_control->surface);
	if (view) {
		/*
		 * tearing_control->current is actually an enum:
		 * WP_TEARING_CONTROL_V1_PRESENTATION_HINT_VSYNC = 0
		 * WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC = 1
		 *
		 * Using it as a bool here allows us to not ship the XML.
		 */
		view->tearing_hint = controller->tearing_control->current;
	}
}

static void
tearing_controller_destroy(struct wl_listener *listener, void *data)
{
	struct tearing_controller *controller = wl_container_of(listener, controller, destroy);
	wl_list_remove(&controller->set_hint.link);
	wl_list_remove(&controller->destroy.link);
	free(controller);
}

void
new_tearing_hint(struct wl_listener *listener, void *data)
{
	struct server *server = wl_container_of(listener, server, tearing_new_object);
	struct wlr_tearing_control_v1 *tearing_control = data;

	enum wp_tearing_control_v1_presentation_hint hint =
		wlr_tearing_control_manager_v1_surface_hint_from_surface
		(server->tearing_control, tearing_control->surface);
	wlr_log(WLR_DEBUG, "New presentation hint %d received for surface %p",
		hint, tearing_control->surface);

	struct tearing_controller *controller = znew(*controller);
	controller->tearing_control = tearing_control;

	controller->set_hint.notify = set_tearing_hint;
	wl_signal_add(&tearing_control->events.set_hint, &controller->set_hint);

	controller->destroy.notify = tearing_controller_destroy;
	wl_signal_add(&tearing_control->events.destroy, &controller->destroy);
}
