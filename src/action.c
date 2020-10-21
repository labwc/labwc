#include <strings.h>

#include "common/log.h"
#include "common/spawn.h"
#include "labwc.h"

void
action(struct server *server, const char *action, const char *command)
{
	if (!action)
		return;
	if (!strcasecmp(action, "Exit")) {
		wl_display_terminate(server->wl_display);
	} else if (!strcasecmp(action, "Execute")) {
		spawn_async_no_shell(command);
	} else if (!strcasecmp(action, "NextWindow")) {
		server->cycle_view =
			desktop_next_view(server, server->cycle_view);
	} else if (!strcasecmp(action, "Reconfigure")) {
		spawn_async_no_shell("killall -SIGHUP labwc");
	} else if (!strcasecmp(action, "Debug")) {
		dbg_show_views(server);
	} else {
		warn("action (%s) not supported", action);
	}
}
