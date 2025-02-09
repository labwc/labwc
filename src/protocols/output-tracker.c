// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>
#include "common/mem.h"
#include "common/list.h"
#include "protocols/output-tracker.h"

struct object_output {
	void *object;
	struct wl_list *object_resources;
	struct wlr_output *wlr_output;
	const struct output_tracker_impl *impl;

	struct {
		struct wl_listener output_bind;
		struct wl_listener output_destroy;
	} on;
	struct wl_list link;
};

static struct wl_list objects = WL_LIST_INIT(&objects);

/* Internal helpers */
static bool
object_output_send_event(struct wl_list *object_resources, struct wl_list *output_resources,
		void (*notifier)(struct wl_resource *object, struct wl_resource *output))
{
	bool sent = false;
	struct wl_client *client;
	struct wl_resource *object_resource, *output_resource;
	wl_resource_for_each(object_resource, object_resources) {
		client = wl_resource_get_client(object_resource);
		wl_resource_for_each(output_resource, output_resources) {
			if (wl_resource_get_client(output_resource) == client) {
				notifier(object_resource, output_resource);
				sent = true;
			}
		}
	}
	return sent;
}

static void
_object_output_destroy(struct object_output *object_output)
{
	object_output_send_event(
		object_output->object_resources,
		&object_output->wlr_output->resources,
		object_output->impl->send_output_leave);

	wl_list_remove(&object_output->link);
	wl_list_remove(&object_output->on.output_bind.link);
	wl_list_remove(&object_output->on.output_destroy.link);

	if (object_output->impl->send_done) {
		object_output->impl->send_done(object_output->object, /*client*/ NULL);
	}

	free(object_output);
}

/* Internal handlers */
static void
handle_output_destroy(struct wl_listener *listener, void *data)
{
	struct object_output *object_output =
		wl_container_of(listener, object_output, on.output_destroy);
	_object_output_destroy(object_output);
}

static void
handle_output_bind(struct wl_listener *listener, void *data)
{
	struct object_output *object_output =
		wl_container_of(listener, object_output, on.output_bind);

	struct wlr_output_event_bind *event = data;
	struct wl_client *client = wl_resource_get_client(event->resource);

	bool sent = false;
	struct wl_resource *object_resource;
	wl_resource_for_each(object_resource, object_output->object_resources) {
		if (wl_resource_get_client(object_resource) == client) {
			object_output->impl->send_output_enter(object_resource, event->resource);
			sent = true;
		}
	}
	if (!sent || !object_output->impl->send_done) {
		return;
	}

	object_output->impl->send_done(object_output->object, client);
}

/* Public API */
void
output_tracker_send_initial_state_to_resource(void *object, struct wl_resource *object_resource)
{
	struct object_output *object_output;
	struct wl_client *client = wl_resource_get_client(object_resource);
	wl_list_for_each(object_output, &objects, link) {
		if (object_output->object != object) {
			continue;
		}
		struct wl_resource *output_resource;
		wl_resource_for_each(output_resource, &object_output->wlr_output->resources) {
			if (wl_resource_get_client(output_resource) != client) {
				continue;
			}
			object_output->impl->send_output_enter(object_resource, output_resource);
		}
	}
}

void
output_tracker_enter(void *object, struct wl_list *object_resources,
		struct wlr_output *wlr_output, const struct output_tracker_impl *impl)
{
	assert(impl);
	assert(impl->send_output_enter);
	assert(impl->send_output_leave);
	struct object_output *object_output;
	wl_list_for_each(object_output, &objects, link) {
		if (object_output->wlr_output == wlr_output
				&& object_output->object == object) {
			/* Object already on given output */
			return;
		}
	}
	object_output = znew(*object_output);
	object_output->object = object;
	object_output->object_resources = object_resources;
	object_output->wlr_output = wlr_output;
	object_output->impl = impl;

	object_output->on.output_bind.notify = handle_output_bind;
	wl_signal_add(&wlr_output->events.bind, &object_output->on.output_bind);

	object_output->on.output_destroy.notify = handle_output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &object_output->on.output_destroy);

	wl_list_insert(&objects, &object_output->link);

	bool sent = object_output_send_event(
		object_resources, &wlr_output->resources, impl->send_output_enter);

	if (sent && impl->send_done) {
		impl->send_done(object, /*client*/ NULL);
	}
}

void
output_tracker_destroy(void *object)
{
	struct object_output *object_output, *tmp;
	wl_list_for_each_safe(object_output, tmp, &objects, link) {
		if (object_output->object == object) {
			_object_output_destroy(object_output);
		}
	}
}

void
output_tracker_leave(void *object, struct wlr_output *wlr_output)
{
	struct object_output *tmp;
	struct object_output *object_output = NULL;
	wl_list_for_each(tmp, &objects, link) {
		if (tmp->object == object && tmp->wlr_output == wlr_output) {
			object_output = tmp;
			break;
		}
	}
	if (object_output) {
		_object_output_destroy(object_output);
	}
}
