// SPDX-License-Identifier: GPL-2.0-only

#include <stdlib.h>
#include <wlr/backend/headless.h>
#include <wlr/types/wlr_output.h>
#include "common/string-helpers.h"
#include "labwc.h"
#include "output-virtual.h"

static struct wlr_output *fallback_output = NULL;

void
output_virtual_add(struct server *server, const char *output_name,
		struct wlr_output **store_wlr_output)
{
	if (output_name) {
		/* Prevent creating outputs with the same name */
		struct output *output;
		wl_list_for_each(output, &server->outputs, link) {
			if (wlr_output_is_headless(output->wlr_output) &&
					!strcmp(output->wlr_output->name, output_name)) {
				wlr_log(WLR_DEBUG,
					"refusing to create virtual output with duplicate name");
				return;
			}
		}
	}

	/*
	 * The headless backend will always emit the new output signal (and
	 * thus call our handler) before `wlr_headless_add_output()` returns.
	 *
	 * This makes it impossible to
	 * - modify the output before it gets enabled in the handler
	 * - use a pointer of the new wlr_output within the handler
	 *
	 * So we temporarily disconnect the handler when creating the output
	 * and then call the handler manually.
	 *
	 * This is important because some operations like `wlr_output_set_name()`
	 * can only be done before the output has been enabled.
	 *
	 * If we add a virtual output before the headless backend has been started
	 * we may end up calling the new output handler twice, one time manually
	 * and one time by the headless backend when it starts up and sends the
	 * signal for all its configured outputs. Rather than keeping a global
	 * server->headless.started state around that we could check here we just
	 * ignore duplicated new output calls in new_output_notify().
	 */
	wl_list_remove(&server->new_output.link);

	struct wlr_output *wlr_output = wlr_headless_add_output(
		server->headless.backend, 1920, 1080);

	if (!wlr_output) {
		wlr_log(WLR_ERROR, "Failed to create virtual output %s",
			output_name ? output_name : "");
		goto restore_handler;
	}

	if (output_name) {
		wlr_output_set_name(wlr_output, output_name);
	}
	if (store_wlr_output) {
		/* Ensures that we can use the new wlr_output pointer within new_output_notify() */
		*store_wlr_output = wlr_output;
	}

	/* Notify about the new output manually */
	if (server->new_output.notify) {
		server->new_output.notify(&server->new_output, wlr_output);
	}

restore_handler:
	/* And finally restore output notifications */
	wl_signal_add(&server->backend->events.new_output, &server->new_output);
}

void
output_virtual_remove(struct server *server, const char *output_name)
{
	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (!wlr_output_is_headless(output->wlr_output)
				|| output->wlr_output == fallback_output) {
			continue;
		}

		if (output_name) {
			/*
			 * Given virtual output name, find and
			 * destroy virtual output by that name.
			 */
			if (!strcmp(output->wlr_output->name, output_name)) {
				wlr_output_destroy(output->wlr_output);
				return;
			}
		} else {
			/*
			 * When virtual output name was not supplied by user,
			 * simply destroy the first virtual output found.
			 */
			wlr_output_destroy(output->wlr_output);
			return;
		}
	}
}

void
output_virtual_update_fallback(struct server *server)
{
	struct wl_list *layout_outputs = &server->output_layout->outputs;
	const char *fallback_output_name = getenv("LABWC_FALLBACK_OUTPUT");

	if (!fallback_output && wl_list_empty(layout_outputs)
			&& !string_null_or_empty(fallback_output_name)) {
		wlr_log(WLR_DEBUG, "adding fallback output %s", fallback_output_name);

		output_virtual_add(server, fallback_output_name, &fallback_output);
	} else if (fallback_output && (wl_list_length(layout_outputs) > 1
			|| string_null_or_empty(fallback_output_name))) {
		wlr_log(WLR_DEBUG, "destroying fallback output %s",
			fallback_output->name);

		/*
		 * We must reset fallback_output to NULL before destroying it.
		 *
		 * Otherwise we may end up trying to destroy the same wlr_output
		 * twice due to wlr_output_destroy() removing the output from the
		 * layout which in turn causes us to be called again.
		 */
		struct wlr_output *wlr_output = fallback_output;
		fallback_output = NULL;
		wlr_output_destroy(wlr_output);
	}
}
