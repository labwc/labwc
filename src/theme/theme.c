#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "theme.h"
#include "theme/theme-dir.h"

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

static void entry(const char *key, const char *value)
{
	if (!key || !value)
		return;
	if (!strcmp(key, "window.active.title.bg.color"))
		parse_hexstr(value, theme.window_active_title_bg_color);
	if (!strcmp(key, "window.active.handle.bg.color"))
		parse_hexstr(value, theme.window_active_handle_bg_color);
	if (!strcmp(key, "window.inactive.title.bg.color"))
		parse_hexstr(value, theme.window_inactive_title_bg_color);
}

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

void theme_read(const char *theme_name)
{
	FILE *stream;
	char *line = NULL;
	size_t len = 0;
	char themerc[4096];

	snprintf(themerc, sizeof(themerc), "%s/themerc", theme_dir(theme_name));
	fprintf(stderr, "info: read themerc (%s)\n", themerc);
	stream = fopen(themerc, "r");
	if (!stream) {
		fprintf(stderr, "warn: cannot read (%s)\n", themerc);
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
