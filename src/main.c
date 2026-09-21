// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <getopt.h>
#include <limits.h>
#include <pango/pangocairo.h>
#include <signal.h>
#include <unistd.h>
#include <wlr/version.h>
#include "common/fd-util.h"
#include "common/font.h"
#include "common/macros.h"
#include "common/spawn.h"
#include "common/string-helpers.h"
#include "config/rcxml.h"
#include "config/session.h"
#include "config/daemonize.h"
#include "labwc.h"
#include "theme.h"
#include "translate.h"
#include "menu/menu.h"

/*
 * Globals
 *
 * Rationale: these are unlikely to ever have more than one instance
 * per process, and need to last for the lifetime of the process.
 * Accessing them indirectly through pointers embedded in every other
 * struct just adds noise to the code.
 */
struct rcxml rc = { 0 };
struct server server = { 0 };

enum {
	OPT_DAEMONIZE_APPLY = 256,
};

static const struct option long_options[] = {
	{"config", required_argument, NULL, 'c'},
	{"config-dir", required_argument, NULL, 'C'},
	{"daemonize-apply", no_argument, NULL, OPT_DAEMONIZE_APPLY},
	{"debug", no_argument, NULL, 'd'},
	{"exit", no_argument, NULL, 'e'},
	{"help", no_argument, NULL, 'h'},
	{"merge-config", no_argument, NULL, 'm'},
	{"reconfigure", no_argument, NULL, 'r'},
	{"startup", required_argument, NULL, 's'},
	{"session", required_argument, NULL, 'S'},
	{"title", required_argument, NULL, 't'},
	{"version", no_argument, NULL, 'v'},
	{"verbose", no_argument, NULL, 'V'},
	{0, 0, 0, 0}
};

static const char labwc_usage[] =
"Usage: labwc [options...]\n"
"  -c, --config <file>      Specify config file (with path)\n"
"  -C, --config-dir <dir>   Specify config directory\n"
"      --daemonize-apply    Apply the <daemonize> setting to the systemd user units and quit\n"
"  -d, --debug              Enable full logging, including debug information\n"
"  -e, --exit               Exit the compositor\n"
"  -h, --help               Show help message and quit\n"
"  -m, --merge-config       Merge user config files/theme in all XDG Base Dirs\n"
"  -r, --reconfigure        Reload the compositor configuration\n"
"  -s, --startup <command>  Run command on startup\n"
"  -S, --session <command>  Run command on startup and terminate on exit\n"
"  -t, --title <fmtstr>     Specify title to use when running in a window\n"
"  -v, --version            Show version number and quit\n"
"  -V, --verbose            Enable more verbose logging\n";

static void
usage(void)
{
	printf("%s", labwc_usage);
	exit(0);
}

static void
print_version(void)
{
	#define FEATURE_ENABLED(feature) (HAVE_##feature ? "+" : "-")
	printf("labwc %s (%sxwayland %snls %srsvg %slibsfdo) wlroots-%d.%d.%d\n",
		LABWC_VERSION,
		FEATURE_ENABLED(XWAYLAND),
		FEATURE_ENABLED(NLS),
		FEATURE_ENABLED(RSVG),
		FEATURE_ENABLED(LIBSFDO),
		wlr_version_get_major(),
		wlr_version_get_minor(),
		wlr_version_get_micro()
	);
	#undef FEATURE_ENABLED
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
die_on_no_fonts(void)
{
	PangoContext *context = pango_font_map_create_context(
		pango_cairo_font_map_get_default());
	PangoLayout *layout = pango_layout_new(context);
	pango_layout_set_text(layout, "abcdefg", -1);
	int nr_unknown_glyphs = pango_layout_get_unknown_glyphs_count(layout);
	g_object_unref(layout);
	g_object_unref(context);

	if (nr_unknown_glyphs > 0) {
		wlr_log(WLR_ERROR, "no fonts are available");
		exit(EXIT_FAILURE);
	}

	/*
	 * Make pango's dedicated thread exit. This prevents CI failures due to
	 * SIGTERM delivered to the pango's thread. This kind of workaround is
	 * not needed after we register our SIGTERM handler in
	 * server_init() > wl_event_loop_add_signal(), which masks SIGTERM.
	 */
	pango_cairo_font_map_set_default(NULL);
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

struct idle_ctx {
	const char *primary_client;
	const char *startup_cmd;
};

static void
idle_callback(void *data)
{
	/* Idle callbacks destroy automatically once triggered */
	struct idle_ctx *ctx = data;

	/* Start session-manager if one is specified by -S|--session */
	if (ctx->primary_client) {
		server.primary_client_pid = spawn_primary_client(ctx->primary_client);
		if (server.primary_client_pid < 0) {
			wlr_log(WLR_ERROR, "fatal error starting primary client: %s",
				ctx->primary_client);
			wl_display_terminate(server.wl_display);
			return;
		}
	}

	session_autostart_init();
	if (ctx->startup_cmd) {
		spawn_async_no_shell(ctx->startup_cmd);
	}
}

static bool
socket_exists(const char *path)
{
	return path && *path && access(path, F_OK) == 0;
}

/*
 * WAYLAND_DISPLAY is either an absolute socket path or a name relative
 * to XDG_RUNTIME_DIR.
 */
static bool
wayland_display_is_stale(const char *display)
{
	if (!display || !*display) {
		return false;
	}
	if (display[0] == '/') {
		return !socket_exists(display);
	}
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir || !*runtime_dir) {
		return true;
	}
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/%s", runtime_dir, display);
	return !socket_exists(path);
}

/*
 * Only local X11 displays can be checked; leave remote ones alone.
 * Local forms are ":0", ":0.0", "unix:0" and "localhost:0".
 */
static bool
x11_display_is_stale(const char *display)
{
	if (!display || !*display) {
		return false;
	}
	const char *colon = strrchr(display, ':');
	if (!colon) {
		return false;
	}
	size_t prefix_len = colon - display;
	if (prefix_len > 0
			&& strncmp(display, "unix", prefix_len)
			&& strncmp(display, "localhost", prefix_len)) {
		return false;
	}
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "/tmp/.X11-unix/X%s", colon + 1);
	char *dot = strchr(path, '.');
	if (dot) {
		*dot = '\0';
	}
	return !socket_exists(path);
}

static void
drop_stale_display_vars(void)
{
	/*
	 * The systemd user manager outlives individual sessions and keeps
	 * whatever environment the previous session imported, including
	 * WAYLAND_DISPLAY and DISPLAY pointing at a compositor that no
	 * longer exists. If we inherit those, wlroots picks the nested
	 * Wayland or X11 backend instead of DRM and we die at startup.
	 *
	 * Only do this when started as a systemd service and only when the
	 * socket the variable points to is actually gone, so that nested
	 * runs against a live compositor keep working.
	 */
	const char *notify_socket = getenv("NOTIFY_SOCKET");
	if (!notify_socket || !*notify_socket) {
		return;
	}
	const char *display = getenv("WAYLAND_DISPLAY");
	if (display && wayland_display_is_stale(display)) {
		wlr_log(WLR_INFO, "removing stale WAYLAND_DISPLAY (%s)", display);
		unsetenv("WAYLAND_DISPLAY");
	}
	display = getenv("DISPLAY");
	if (display && x11_display_is_stale(display)) {
		wlr_log(WLR_INFO, "removing stale DISPLAY (%s)", display);
		unsetenv("DISPLAY");
	}
}

int
main(int argc, char *argv[])
{
	char *startup_cmd = NULL;
	char *primary_client = NULL;
	bool apply_daemonize_only = false;
	enum wlr_log_importance verbosity = WLR_ERROR;

	server.wlr_version = _LAB_CALC_WLR_VERSION_NUM(
		wlr_version_get_major(),
		wlr_version_get_minor(),
		wlr_version_get_micro()
	);

	int c;
	while (1) {
		int index = 0;
		c = getopt_long(argc, argv, "c:C:dehmrs:S:t:vV", long_options, &index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'c':
			rc.config_file = optarg;
			break;
		case 'C':
			rc.config_dir = optarg;
			break;
		case OPT_DAEMONIZE_APPLY:
			apply_daemonize_only = true;
			break;
		case 'd':
			verbosity = WLR_DEBUG;
			break;
		case 'e':
			send_signal_to_labwc_pid(SIGTERM);
			exit(0);
		case 'm':
			rc.merge_config = true;
			break;
		case 'r':
			send_signal_to_labwc_pid(SIGHUP);
			exit(0);
		case 's':
			startup_cmd = optarg;
			break;
		case 'S':
			primary_client = optarg;
			break;
		case 't':
			server.title_fmt = optarg;
			break;
		case 'v':
			print_version();
			exit(0);
		case 'V':
			verbosity = WLR_INFO;
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
	if (!apply_daemonize_only) {
		die_on_no_fonts();
	}

	session_environment_init();

#if HAVE_NLS
	/* Initialize locale after setting env vars */
	setlocale(LC_ALL, "");
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	textdomain(GETTEXT_PACKAGE);
#endif

	rcxml_read(rc.config_file);
	wlr_log(WLR_INFO, "daemonize enabled: %d", rc.daemonize_enabled);
	daemonize_apply(rc.daemonize_enabled);

	if (apply_daemonize_only) {
		/*
		 * Write (or remove) the systemd user units according to the
		 * <daemonize> configuration item and quit without starting the
		 * compositor. This lets the labwc-session launcher create the
		 * units before starting labwc.service, so that enabling
		 * daemonize takes effect on the first login.
		 */
		exit(EXIT_SUCCESS);
	}

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

	/* useful for helper programs */
	if (setenv("LABWC_VER", LABWC_VERSION, true) < 0) {
		wlr_log_errno(WLR_ERROR, "unable to set LABWC_VER");
	} else {
		wlr_log(WLR_DEBUG, "LABWC_VER=%s", LABWC_VERSION);
	}

	if (!getenv("XDG_RUNTIME_DIR")) {
		wlr_log(WLR_ERROR, "XDG_RUNTIME_DIR is unset");
		exit(EXIT_FAILURE);
	}

	increase_nofile_limit();

	if (string_null_or_empty(server.title_fmt)) {
		server.title_fmt = "labwc - %o";
	}

	drop_stale_display_vars();

	server_init();
	server_start();
	daemonize_notify_ready();

	struct theme theme = { 0 };
	theme_init(&theme, rc.theme_name);
	rc.theme = &theme;

	menu_init();

	/* Delay startup of applications until the event loop is ready */
	struct idle_ctx idle_ctx = {
		.primary_client = primary_client,
		.startup_cmd = startup_cmd
	};
	wl_event_loop_add_idle(server.wl_event_loop, idle_callback, &idle_ctx);

	wl_display_run(server.wl_display);

	session_shutdown();

	menu_finish();
	theme_finish(&theme);
	rcxml_finish();
	font_finish();

	server_finish();

	return 0;
}
