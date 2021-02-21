#include "common/font.h"
#include "common/log.h"
#include "common/spawn.h"
#include "config/session.h"
#include "labwc.h"
#include "theme/theme.h"
#include "xbm/xbm.h"
#include "menu/menu.h"

struct rcxml rc = { 0 };
struct theme theme = { 0 };

static const char labwc_usage[] =
	"Usage: labwc [-h] [-s <startup-command>] [-c <config-file>] [-v]\n";

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
	enum verbosity {
		LAB_VERBOSITY_ERROR = 0,
		LAB_VERBOSITY_INFO,
		LAB_VERBOSITY_DEBUG,
	} verbosity = LAB_VERBOSITY_ERROR;

	int c;
	while ((c = getopt(argc, argv, "c:s:hv")) != -1) {
		switch (c) {
		case 'c':
			config_file = optarg;
			break;
		case 's':
			startup_cmd = optarg;
			break;
		case 'v':
			++verbosity;
			break;
		case 'h':
		default:
			usage();
		}
	}
	if (optind < argc) {
		usage();
	}

	switch (verbosity) {
	case LAB_VERBOSITY_ERROR:
		wlr_log_init(WLR_ERROR, NULL);
		break;
	case LAB_VERBOSITY_INFO:
		wlr_log_init(WLR_INFO, NULL);
		break;
	case LAB_VERBOSITY_DEBUG:
	default:
		wlr_log_init(WLR_DEBUG, NULL);
		break;
	}

	session_environment_init();
	rcxml_read(config_file);

	if (!getenv("XDG_RUNTIME_DIR")) {
		die("XDG_RUNTIME_DIR is unset");
	}

	struct server server = { 0 };
	server_init(&server);
	server_start(&server);

	theme_init(&theme, server.renderer, rc.theme_name);
	server.theme = &theme;

	struct menu rootmenu = { 0 };
	menu_init_rootmenu(&server, &rootmenu);

	session_autostart_init();
	if (startup_cmd) {
		spawn_async_no_shell(startup_cmd);
	}

	wl_display_run(server.wl_display);

	server_finish(&server);

	menu_finish(&rootmenu);
	theme_finish(&theme);
	rcxml_finish();
	font_finish();
	return 0;
}
