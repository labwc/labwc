#include "common/spawn.h"
#include "labwc.h"
#include "theme/theme.h"
#include "xbm/xbm.h"

#include <cairo.h>
#include <pango/pangocairo.h>

struct rcxml rc = { 0 };
struct theme theme = { 0 };

static const char labwc_usage[] =
	"Usage: labwc [-h] [-s <startup-command>] [-c <config-file>]\n";

static void
usage(void)
{
	printf("%s", labwc_usage);
	exit(0);
}

int
main(int argc, char *argv[])
{
	char *startup_cmd = NULL;
	char *config_file = NULL;

	int c;
	while ((c = getopt(argc, argv, "c:s:h")) != -1) {
		switch (c) {
		case 'c':
			config_file = optarg;
			break;
		case 's':
			startup_cmd = optarg;
			break;
		case 'h':
		default:
			usage();
		}
	}
	if (optind < argc) {
		usage();
	}

	wlr_log_init(WLR_ERROR, NULL);
	rcxml_read(config_file);

	if (!getenv("XDG_RUNTIME_DIR")) {
		wlr_log(WLR_ERROR, "XDG_RUNTIME_DIR is required to be set");
		return 1;
	}

	struct server server = { 0 };
	server_init(&server);
	server_start(&server);

	theme_read(rc.theme_name);
	xbm_load(server.renderer);

	if (startup_cmd) {
		spawn_async_no_shell(startup_cmd);
	}

	wl_display_run(server.wl_display);

	server_finish(&server);
	rcxml_finish();
	pango_cairo_font_map_set_default(NULL);
	return 0;
}
