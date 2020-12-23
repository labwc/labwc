#include <glib.h>

void
spawn_async_no_shell(char const *command)
{
	GError *err = NULL;
	gchar **argv = NULL;

	g_shell_parse_argv((gchar *)command, NULL, &argv, &err);
	if (err) {
		g_message("%s", err->message);
		g_error_free(err);
		return;
	}
	g_spawn_async(NULL, argv, NULL,
		G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL,
		NULL, &err);
	if (err) {
		g_message("%s", err->message);
		g_error_free(err);
	}
	g_strfreev(argv);
}
