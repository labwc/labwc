#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/dir.h"
#include "common/log.h"
#include "common/string-helpers.h"
#include "theme/theme.h"

static int
hex_to_dec(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return 0;
}

void
parse_hexstr(const char *hex, float *rgba)
{
	if (!hex || hex[0] != '#' || strlen(hex) < 7) {
		return;
	}
	rgba[0] = (hex_to_dec(hex[1]) * 16 + hex_to_dec(hex[2])) / 255.0;
	rgba[1] = (hex_to_dec(hex[3]) * 16 + hex_to_dec(hex[4])) / 255.0;
	rgba[2] = (hex_to_dec(hex[5]) * 16 + hex_to_dec(hex[6])) / 255.0;
	if (strlen(hex) > 7) {
		rgba[3] = atoi(hex + 7) / 100.0;
	} else {
		rgba[3] = 1.0;
	}
}

static bool
match(const gchar *pattern, const gchar *string)
{
	return (bool)g_pattern_match_simple(pattern, string);
}

static void entry(const char *key, const char *value)
{
	if (!key || !value) {
		return;
	}
	if (match(key, "window.active.title.bg.color")) {
		parse_hexstr(value, theme.window_active_title_bg_color);
	} else if (match(key, "window.active.handle.bg.color")) {
		parse_hexstr(value, theme.window_active_handle_bg_color);
	} else if (match(key, "window.inactive.title.bg.color")) {
		parse_hexstr(value, theme.window_inactive_title_bg_color);
	} else if (match(key, "window.active.button.unpressed.image.color")) {
		parse_hexstr(value, theme.window_active_button_unpressed_image_color);
	} else if (match(key, "window.inactive.button.unpressed.image.color")) {
		parse_hexstr(value, theme.window_inactive_button_unpressed_image_color);
	} else if (match(key, "menu.items.bg.color")) {
		parse_hexstr(value, theme.menu_items_bg_color);
	} else if (match(key, "menu.items.text.color")) {
		parse_hexstr(value, theme.menu_items_text_color);
	} else if (match(key, "menu.items.active.bg.color")) {
		parse_hexstr(value, theme.menu_items_active_bg_color);
	} else if (match(key, "menu.items.active.text.color")) {
		parse_hexstr(value, theme.menu_items_active_text_color);
	}
}

static void
parse_config_line(char *line, char **key, char **value)
{
	char *p = strchr(line, ':');
	if (!p) {
		return;
	}
	*p = '\0';
	*key = string_strip(line);
	*value = string_strip(++p);
}

static void
process_line(char *line)
{
	if (line[0] == '\0' || line[0] == '#') {
		return;
	}
	char *key = NULL, *value = NULL;
	parse_config_line(line, &key, &value);
	entry(key, value);
}

void
theme_read(const char *theme_name)
{
	FILE *stream = NULL;
	char *line = NULL;
	size_t len = 0;
	char themerc[4096];

	if (strlen(theme_dir(theme_name))) {
		snprintf(themerc, sizeof(themerc), "%s/themerc",
			 theme_dir(theme_name));
		stream = fopen(themerc, "r");
	}
	if (!stream) {
		info("cannot find theme (%s), using built-in", theme_name);
		theme_builtin();
		return;
	}
	info("read themerc (%s)", themerc);
	while (getline(&line, &len, stream) != -1) {
		char *p = strrchr(line, '\n');
		if (p) {
			*p = '\0';
		}
		process_line(line);
	}
	free(line);
	fclose(stream);
}
