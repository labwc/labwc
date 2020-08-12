#include "labwc.h"
#include "common/spawn.h"
#include "common/log.h"

#include <strings.h>

void action(struct server *server, struct keybind *keybind)
{
	if (!keybind || !keybind->action)
		return;
	if (!strcasecmp(keybind->action, "Exit")) {
		wl_display_terminate(server->wl_display);
	} else if (!strcasecmp(keybind->action, "NextWindow")) {
		server->cycle_view = next_toplevel(view_front_toplevel(server));
	} else if (!strcasecmp(keybind->action, "Execute")) {
		spawn_async_no_shell(keybind->command);
	} else if (!strcasecmp(keybind->action, "debug-views")) {
		dbg_show_views(server);
	} else {
		warn("action (%s) not supported", keybind->action);
	}
}
