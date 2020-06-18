#include "labwc.h"

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
		if (!fork())
			execl("/bin/dmenu_run", "/bin/dmenu_run", (void *)NULL);
	} else if (!strcasecmp(keybind->action, "debug-views")) {
		dbg_show_views(server);
	} else {
		fprintf(stderr, "warn: action (%s) not supported\n",
			keybind->action);
	}
}
