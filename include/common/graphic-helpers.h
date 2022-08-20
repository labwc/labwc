/* SPDX-License-Identifier: GPL-2.0-only */

struct wlr_scene_tree;
struct wlr_scene_rect;
struct wl_listener;

struct multi_rect {
	struct wlr_scene_tree *tree;
	int line_width; /* read-only */

	/* Private */
	struct wlr_scene_rect *top[3];
	struct wlr_scene_rect *bottom[3];
	struct wlr_scene_rect *left[3];
	struct wlr_scene_rect *right[3];
	struct wl_listener destroy;
};

/**
 * Create a new multi_rect.
 * A multi_rect consists of 3 nested rectangular outlines.
 * Each of the rectangular outlines is using the same @line_width
 * but its own color based on the @colors argument.
 *
 * The multi-rect can be positioned by positioning multi_rect->tree->node.
 *
 * It can be destroyed by destroying its tree node (or one of its
 * parent nodes). Once the tree node has been destroyed the struct
 * will be free'd automatically.
 */
struct multi_rect *multi_rect_create(struct wlr_scene_tree *parent,
		float *colors[3], int line_width);

void multi_rect_set_size(struct multi_rect *rect, int width, int height);

/**
 * Sets the cairo color.
 * Splits a float[4] single color array into its own arguments
 */
void set_cairo_color(cairo_t *cairo, float *color);

/* Draws a border with a specified line width */
void draw_cairo_border(cairo_t *cairo, double width, double height,
		double line_width);
