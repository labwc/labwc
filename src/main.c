#include "labwc.h"
#include "theme/theme.h"
#include "xbm/xbm.h"
#include "common/spawn.h"

#include <cairo.h>
#include <pango/pangocairo.h>

struct server server = { 0 };
struct rcxml rc = { 0 };
struct theme theme = { 0 };

static const char labwc_usage[] =
	"Usage: labwc [-h] [-s <startup-command>] [-c <config-file>]\n";

static void usage(void)
{
	printf("%s", labwc_usage);
	exit(0);
}

int main(int argc, char *argv[])
{
	char *startup_cmd = NULL;
	char *config_file = NULL;
	wlr_log_init(WLR_ERROR, NULL);

	int c;
	while ((c = getopt(argc, argv, "s:c:h")) != -1) {
		switch (c) {
		case 's':
			startup_cmd = optarg;
			break;
		case 'c':
			config_file = optarg;
			break;
		default:
			usage();
		}
	}
	if (optind < argc)
		usage();

	rcxml_read(config_file);

	/* Wayland requires XDG_RUNTIME_DIR to be set */
	if (!getenv("XDG_RUNTIME_DIR")) {
		wlr_log(WLR_ERROR, "XDG_RUNTIME_DIR is not set");
		return 1;
	}

	/* TODO: Catch signals. Maybe SIGHUP for reconfigure */

	server_init(&server);
	server_start(&server);

	theme_read(rc.theme_name);
	xbm_load(server.renderer);

	if (startup_cmd)
		spawn_async_no_shell(startup_cmd);

	wl_display_run(server.wl_display);
	server_finish(&server);
	rcxml_finish();
	pango_cairo_font_map_set_default(NULL);
	return 0;
}
