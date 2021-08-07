#ifndef __LABWC_FONT_H
#define __LABWC_FONT_H

struct server;
struct wlr_texture;
struct wlr_box;

/**
 * font_height - get font vertical extents
 * @font_description: string describing font, for example 'sans 10'
 */
int font_height(const char *font_description);

/**
 * texture_create - Create ARGB8888 texture using pango
 * @server: context (for wlr_renderer)
 * @texture: texture pointer; existing pointer will be freed
 * @max_width: max allowable width; will be ellipsized if longer
 * @text: text to be generated as texture
 * @font: font description
 * @color: foreground color in rgba format
 */
void font_texture_create(struct server *server, struct wlr_texture **texture,
	int max_width, const char *text, const char *font, float *color);

/**
 * font_finish - free some font related resources
 * Note: use on exit
 */
void font_finish(void);

#endif /* __LABWC_FONT_H */
