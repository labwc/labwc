#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <glib.h>

#include "theme/theme.h"
#include "common/dir.h"
#include "common/log.h"

static int hex_to_dec(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return 0;
}

static void parse_hexstr(const char *hex, float *rgba)
{
	if (!hex || hex[0] != '#' || strlen(hex) < 7)
		return;
	rgba[0] = (hex_to_dec(hex[1]) * 16 + hex_to_dec(hex[2])) / 255.0;
	rgba[1] = (hex_to_dec(hex[3]) * 16 + hex_to_dec(hex[4])) / 255.0;
	rgba[2] = (hex_to_dec(hex[5]) * 16 + hex_to_dec(hex[6])) / 255.0;
	if (strlen(hex) > 7)
		rgba[3] = atoi(hex + 7) / 100.0;
	else
		rgba[3] = 1.0;
}

static bool match(const gchar *pattern, const gchar *string)
{
	return (bool)g_pattern_match_simple(pattern, string);
}

/* clang-format off */
static void entry(const char *key, const char *value)
{
	if (!key || !value)
		return;
	if (match(key, "window.active.title.bg.color"))
		parse_hexstr(value, theme.window_active_title_bg_color);
	if (match(key, "window.active.handle.bg.color"))
		parse_hexstr(value, theme.window_active_handle_bg_color);
	if (match(key, "window.inactive.title.bg.color"))
		parse_hexstr(value, theme.window_inactive_title_bg_color);
	if (match(key, "window.active.button.unpressed.image.color"))
		parse_hexstr(value, theme.window_active_button_unpressed_image_color);
	if (match(key, "window.inactive.button.unpressed.image.color"))
		parse_hexstr(value, theme.window_inactive_button_unpressed_image_color);
}
/* clang-format on */

static void rtrim(char **s)
{
	size_t len = strlen(*s);
	if (!len)
		return;
	char *end = *s + len - 1;
	while (end >= *s && isspace(*end))
		end--;
	*(end + 1) = '\0';
}

static char *strstrip(char *s)
{
	rtrim(&s);
	while (isspace(*s))
		s++;
	return s;
}

static void parse_config_line(char *line, char **key, char **value)
{
	char *p = strchr(line, ':');
	if (!p)
		return;
	*p = '\0';
	*key = strstrip(line);
	*value = strstrip(++p);
}

static void process_line(char *line)
{
	if (line[0] == '\0' || line[0] == '#')
		return;
	char *key = NULL, *value = NULL;
	parse_config_line(line, &key, &value);
	entry(key, value);
}

/*
 * We generally use Openbox defaults, but if no theme file can be found it's
 * better to populate the theme variables with some sane values as no-one
 * wants to use openbox without a theme - it'll all just be black and white.
 *
 * Openbox doesn't actual start if it can't find a theme. As it's normally
 * packaged with Clearlooks, this is not a problem, but for labwc I thought
 * this was a bit hard-line. People might want to try labwc without having
 * Openbox (and associated themes) installed.
 */
/* clang-format off */
void theme_builtin(void)
{
	parse_hexstr("#589bda", theme.window_active_title_bg_color);
	parse_hexstr("#3c7cb7", theme.window_active_handle_bg_color);
	parse_hexstr("#efece6", theme.window_inactive_title_bg_color);
	parse_hexstr("#ffffff", theme.window_active_button_unpressed_image_color);
	parse_hexstr("#000000", theme.window_inactive_button_unpressed_image_color);
}
/* clang-format on */

void theme_read(const char *theme_name)
{
	FILE *stream;
	char *line = NULL;
	size_t len = 0;
	char themerc[4096];

	snprintf(themerc, sizeof(themerc), "%s/themerc", theme_dir(theme_name));
	info("reading themerc (%s)", themerc);
	stream = fopen(themerc, "r");
	if (!stream) {
		warn("cannot read (%s) - load built-in theme", themerc);
		theme_builtin();
		return;
	}
	while (getline(&line, &len, stream) != -1) {
		char *p = strrchr(line, '\n');
		if (p)
			*p = '\0';
		process_line(line);
	}
	free(line);
	fclose(stream);
}
