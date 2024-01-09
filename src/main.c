// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "common/dir.h"
#include "common/fd_util.h"
#include "common/font.h"
#include "common/mem.h"
#include "common/spawn.h"
#include "common/string-helpers.h"
#include "config/session.h"
#include "labwc.h"
#include "theme.h"
#include "menu/menu.h"

struct rcxml rc = { 0 };

static const struct option long_options[] = {
	{"config", required_argument, NULL, 'c'},
	{"config-dir", required_argument, NULL, 'C'},
	{"debug", no_argument, NULL, 'd'},
	{"exit", no_argument, NULL, 'e'},
	{"help", no_argument, NULL, 'h'},
	{"merge-config", no_argument, NULL, 'm'},
	{"reconfigure", no_argument, NULL, 'r'},
	{"startup", required_argument, NULL, 's'},
	{"version", no_argument, NULL, 'v'},
	{"verbose", no_argument, NULL, 'V'},
	{0, 0, 0, 0}
};

static const char labwc_usage[] =
"Usage: labwc [options...]\n"
"  -c, --config <file>      Specify config file (with path)\n"
"  -C, --config-dir <dir>   Specify config directory\n"
"  -d, --debug              Enable full logging, including debug information\n"
"  -e, --exit               Exit the compositor\n"
"  -h, --help               Show help message and quit\n"
"  -m, --merge-config       Merge user rc.xml with system rc.xml\n"
"  -r, --reconfigure        Reload the compositor configuration\n"
"  -s, --startup <command>  Run command on startup\n"
"  -v, --version            Show version number and quit\n"
"  -V, --verbose            Enable more verbose logging\n";

static void
usage(void)
{
	printf("%s", labwc_usage);
	exit(0);
}

static void
die_on_detecting_suid(void)
{
	if (geteuid() != 0 && getegid() != 0) {
		return;
	}
	if (getuid() == geteuid() && getgid() == getegid()) {
		return;
	}
	wlr_log(WLR_ERROR, "SUID detected - aborting");
	exit(EXIT_FAILURE);
}

static void
send_signal_to_labwc_pid(int signal)
{
	char *labwc_pid = getenv("LABWC_PID");
	if (!labwc_pid) {
		wlr_log(WLR_ERROR, "LABWC_PID not set");
		exit(EXIT_FAILURE);
	}
	int pid = atoi(labwc_pid);
	if (!pid) {
		wlr_log(WLR_ERROR, "should not send signal to pid 0");
		exit(EXIT_FAILURE);
	}
	kill(pid, signal);
}

int
main(int argc, char *argv[])
{
#if HAVE_NLS
	setlocale(LC_ALL, "");
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	textdomain(GETTEXT_PACKAGE);
#endif
	char *startup_cmd = NULL;
	char *config_file = NULL;
	enum wlr_log_importance verbosity = WLR_ERROR;
	bool merge_rc = false;

	int c;
	while (1) {
		int index = 0;
		c = getopt_long(argc, argv, "c:C:dehmrs:vV", long_options, &index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'c':
			config_file = optarg;
			break;
		case 'C':
			rc.config_dir = xstrdup(optarg);
			break;
		case 'd':
			verbosity = WLR_DEBUG;
			break;
		case 'e':
			send_signal_to_labwc_pid(SIGTERM);
			exit(0);
		case 'r':
			send_signal_to_labwc_pid(SIGHUP);
			exit(0);
		case 's':
			startup_cmd = optarg;
			break;
		case 'v':
			printf("labwc " LABWC_VERSION "\n");
			exit(0);
		case 'V':
			verbosity = WLR_INFO;
			break;
		case 'm':
			merge_rc = true;
			break;
		case 'h':
		default:
			usage();
		}
	}
	if (optind < argc) {
		usage();
	}

	wlr_log_init(verbosity, NULL);

	die_on_detecting_suid();

	if (!rc.config_dir) {
		rc.config_dir = config_dir();
		if (!merge_rc && !config_file) {
			config_file = strdup_printf("%s/rc.xml", rc.config_dir);
		}
	} else if (!config_file) {
		config_file = strdup_printf("%s/rc.xml", rc.config_dir);
	}

	wlr_log(WLR_INFO, "using config dir (%s)\n", rc.config_dir);
	session_environment_init();
	rcxml_read(config_file);

	/*
	 * Set environment variable LABWC_PID to the pid of the compositor
	 * so that SIGHUP and SIGTERM can be sent to specific instances using
	 * `kill -s <signal> <pid>` rather than `killall -s <signal> labwc`
	 */
	char pid[32];
	snprintf(pid, sizeof(pid), "%d", getpid());
	if (setenv("LABWC_PID", pid, true) < 0) {
		wlr_log_errno(WLR_ERROR, "unable to set LABWC_PID");
	} else {
		wlr_log(WLR_DEBUG, "LABWC_PID=%s", pid);
	}

	if (!getenv("XDG_RUNTIME_DIR")) {
		wlr_log(WLR_ERROR, "XDG_RUNTIME_DIR is unset");
		exit(EXIT_FAILURE);
	}

	increase_nofile_limit();

	struct server server = { 0 };
	server_init(&server);
	server_start(&server);

	struct theme theme = { 0 };
	theme_init(&theme, rc.theme_name);
	rc.theme = &theme;
	server.theme = &theme;

	menu_init(&server);

	session_autostart_init();
	if (startup_cmd) {
		spawn_async_no_shell(startup_cmd);
	}

	wl_display_run(server.wl_display);

	server_finish(&server);

	menu_finish(&server);
	theme_finish(&theme);
	rcxml_finish();
	font_finish();
	return 0;
}
