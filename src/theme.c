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
#include "common/zfree.h"
#include "theme.h"
#include "xbm/xbm.h"

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

/**
 * parse_hexstr - parse #rrggbb
 * @hex: hex string to be parsed
 * @rgba: pointer to float[4] for return value
 */
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

/*
 * We generally use Openbox defaults, but if no theme file can be found it's
 * better to populate the theme variables with some sane values as no-one
 * wants to use openbox without a theme - it'll all just be black and white.
 *
 * Openbox doesn't actual start if it can't find a theme. As it's normally
 * packaged with Clearlooks, this is not a problem, but for labwc I thought
 * this was a bit hard-line. People might want to try labwc without having
 * Openbox (and associated themes) installed.
 *
 * theme_builtin() applies a theme very similar to Clearlooks-3.4
 */
void theme_builtin(struct theme *theme)
{
	theme->border_width = 1;

	parse_hexstr("#3c7cb7", theme->window_active_border_color);
	parse_hexstr("#efece6", theme->window_inactive_border_color);

	parse_hexstr("#589bda", theme->window_active_title_bg_color);
	parse_hexstr("#efece6", theme->window_inactive_title_bg_color);

	parse_hexstr("#ffffff", theme->window_active_button_unpressed_image_color);
	parse_hexstr("#000000", theme->window_inactive_button_unpressed_image_color);

	parse_hexstr("#fcfbfa", theme->menu_items_bg_color);
	parse_hexstr("#000000", theme->menu_items_text_color);
	parse_hexstr("#4a90d9", theme->menu_items_active_bg_color);
	parse_hexstr("#ffffff", theme->menu_items_active_text_color);
}

static bool
match(const gchar *pattern, const gchar *string)
{
	return (bool)g_pattern_match_simple(pattern, string);
}

static void entry(struct theme *theme, const char *key, const char *value)
{
	if (!key || !value) {
		return;
	}

	/*
	 * Note that in order for the pattern match to apply to more than just
	 * the first instance, "else if" cannot be used throughout this function
	 */
	if (match(key, "border.width")) {
		theme->border_width = atoi(value);
	}

	if (match(key, "window.active.border.color")) {
		parse_hexstr(value, theme->window_active_border_color);
	}
	if (match(key, "window.inactive.border.color")) {
		parse_hexstr(value, theme->window_inactive_border_color);
	}

	if (match(key, "window.active.title.bg.color")) {
		parse_hexstr(value, theme->window_active_title_bg_color);
	}
	if (match(key, "window.inactive.title.bg.color")) {
		parse_hexstr(value, theme->window_inactive_title_bg_color);
	}

	if (match(key, "window.active.button.unpressed.image.color")) {
		parse_hexstr(value, theme->window_active_button_unpressed_image_color);
	}
	if (match(key, "window.inactive.button.unpressed.image.color")) {
		parse_hexstr(value, theme->window_inactive_button_unpressed_image_color);
	}

	if (match(key, "menu.items.bg.color")) {
		parse_hexstr(value, theme->menu_items_bg_color);
	}
	if (match(key, "menu.items.text.color")) {
		parse_hexstr(value, theme->menu_items_text_color);
	}
	if (match(key, "menu.items.active.bg.color")) {
		parse_hexstr(value, theme->menu_items_active_bg_color);
	}
	if (match(key, "menu.items.active.text.color")) {
		parse_hexstr(value, theme->menu_items_active_text_color);
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
process_line(struct theme *theme, char *line)
{
	if (line[0] == '\0' || line[0] == '#') {
		return;
	}
	char *key = NULL, *value = NULL;
	parse_config_line(line, &key, &value);
	entry(theme, key, value);
}

static void
theme_read(struct theme *theme, const char *theme_name)
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
		theme_builtin(theme);
		return;
	}
	info("read themerc (%s)", themerc);
	while (getline(&line, &len, stream) != -1) {
		char *p = strrchr(line, '\n');
		if (p) {
			*p = '\0';
		}
		process_line(theme, line);
	}
	free(line);
	fclose(stream);
}

void
theme_init(struct theme *theme, struct wlr_renderer *renderer,
		const char *theme_name)
{
	theme_read(theme, theme_name);
	xbm_load(theme, renderer);
}

void
theme_finish(struct theme *theme)
{
	zfree(theme->xbm_close_active_unpressed);
	zfree(theme->xbm_maximize_active_unpressed);
	zfree(theme->xbm_iconify_active_unpressed);
	zfree(theme->xbm_close_inactive_unpressed);
	zfree(theme->xbm_maximize_inactive_unpressed);
	zfree(theme->xbm_iconify_inactive_unpressed);
}
