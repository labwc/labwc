#include "labwc.h"

struct server server = { 0 };

int main(int argc, char *argv[])
{
	char *startup_cmd = NULL;
	wlr_log_init(WLR_ERROR, NULL);

	int c;
	while ((c = getopt(argc, argv, "s:h")) != -1) {
		switch (c) {
		case 's':
			startup_cmd = optarg;
			break;
		default:
			printf("Usage: %s [-s startup command]\n", argv[0]);
			return 0;
		}
	}
	if (optind < argc) {
		printf("Usage: %s [-s startup command]\n", argv[0]);
		return 0;
	}

	/* Wayland requires XDG_RUNTIME_DIR to be set */
	if (!getenv("XDG_RUNTIME_DIR")) {
		wlr_log(WLR_ERROR, "XDG_RUNTIME_DIR is not set");
		return 1;
	}

	/* TODO: Catch signals. Maybe SIGHUP for reconfigure */

	server_init(&server);
	server_start(&server);

	if (startup_cmd) {
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd,
			      (void *)NULL);
		}
	}
	wl_display_run(server.wl_display);
	server_finish(&server);
	return 0;
}
