// SPDX-License-Identifier: GPL-2.0-only
/*
 * SSD handle and grip assembly for the bottom of the window frame.
 *
 * Implements Openbox-compatible handle/grip decorations with interactive
 * visual states (hover, pressed, dragging).
 */

#include <assert.h>
#include <wlr/render/pixman.h>
#include <wlr/types/wlr_scene.h>
#include "buffer.h"
#include "common/macros.h"
#include "common/scene-helpers.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "node.h"
#include "ssd.h"
#include "ssd-internal.h"
#include "theme.h"
#include "view.h"

/* Inset shadow line width for pressed/dragging state */
#define INSET_LINE_WIDTH 1

/* Overlay colors (premultiplied RGBA) */
static const float overlay_hover[4]   = { 0.0f, 0.0f, 0.0f, 0.20f };
static const float overlay_pressed[4] = { 0.0f, 0.0f, 0.0f, 0.40f };
static const float overlay_none[4]    = { 0.0f, 0.0f, 0.0f, 0.0f };

/* Inset shadow colors (premultiplied) */
static const float inset_dark[4]  = { 0.0f, 0.0f, 0.0f, 0.30f };
static const float inset_light[4] = { 0.10f, 0.10f, 0.10f, 0.10f };

/* Map element index -> node type for hit detection */
static const enum lab_node_type elem_node_types[SSD_HANDLE_ELEMENT_COUNT] = {
	[SSD_HANDLE_ELEMENT_GRIP_LEFT]  = LAB_NODE_GRIP_LEFT,
	[SSD_HANDLE_ELEMENT_CENTER]     = LAB_NODE_HANDLE,
	[SSD_HANDLE_ELEMENT_GRIP_RIGHT] = LAB_NODE_GRIP_RIGHT,
};

/* Spec for a border/separator rect: geometry + node descriptor type */
struct rect_spec {
	int x, y, w, h;
	enum lab_node_type desc_type;
};

/* Per-element geometry (shared by textures, overlays, insets) */
struct elem_geometry {
	int x, y, w, h;
};

/*
 * Compute the layout metrics for the handle/grip assembly.
 * All coordinates are relative to the handle tree origin, which is
 * positioned at (-border_width, effective_height) relative to the
 * SSD root.
 */
struct handle_layout {
	int bw;          /* border_width */
	int hw;          /* handle_width (height of handle content) */
	int full_width;  /* total width including side borders */
	int grip_w;      /* width of each grip */
	int handle_w;    /* width of center handle (between grips) */
	int content_h;   /* height of textured content (= hw, per Openbox spec) */
	int total_h;     /* total assembly height: 2*bw + hw */
	/* X positions (relative to handle tree) */
	int grip_left_x;
	int handle_x;
	int grip_right_x;
	/* Y position of content (below top border line) */
	int content_y;
};

static struct handle_layout
compute_layout(struct theme *theme, int view_width)
{
	struct handle_layout l;
	l.bw = theme->border_width;
	l.hw = theme->handle_width;
	l.full_width = view_width + 2 * l.bw;
	l.grip_w = theme->grip_width;

	/* Ensure grips don't overflow the available width */
	if (2 * l.grip_w + 4 * l.bw > l.full_width) {
		l.grip_w = MAX((l.full_width - 4 * l.bw) / 2, 0);
	}

	/*
	 * Center handle width: total minus outer borders (2*bw),
	 * grips (2*grip_w), and inner separators (2*bw).
	 */
	l.handle_w = MAX(l.full_width - 4 * l.bw - 2 * l.grip_w, 0);

	/* X coordinates accounting for separators between grips and handle */
	l.grip_left_x = l.bw;
	l.handle_x = l.bw + l.grip_w + l.bw;
	l.grip_right_x = l.bw + l.grip_w + l.bw + l.handle_w + l.bw;

	/* Content Y starts after top border line */
	l.content_y = l.bw;

	/*
	 * Per Openbox spec, handle_width is the content-only height.
	 * Borders are drawn around it, not subtracted from it.
	 */
	l.content_h = l.hw;
	l.total_h = 2 * l.bw + l.hw;

	return l;
}

static void
compute_rect_specs(const struct handle_layout *l,
	struct rect_spec specs[HRECT_COUNT])
{
	specs[HRECT_BORDER_LEFT] = (struct rect_spec){
		0, 0, l->bw, l->total_h, LAB_NODE_GRIP_LEFT };
	specs[HRECT_BORDER_RIGHT] = (struct rect_spec){
		l->full_width - l->bw, 0, l->bw, l->total_h,
		LAB_NODE_GRIP_RIGHT };
	specs[HRECT_BORDER_BOTTOM_LEFT] = (struct rect_spec){
		0, l->bw + l->hw, l->bw + l->grip_w + l->bw, l->bw,
		LAB_NODE_GRIP_LEFT };
	specs[HRECT_BORDER_BOTTOM_CENTER] = (struct rect_spec){
		l->handle_x, l->bw + l->hw, l->handle_w, l->bw,
		LAB_NODE_HANDLE };
	specs[HRECT_BORDER_BOTTOM_RIGHT] = (struct rect_spec){
		l->grip_right_x - l->bw, l->bw + l->hw,
		l->bw + l->grip_w + l->bw, l->bw, LAB_NODE_GRIP_RIGHT };
	specs[HRECT_BORDER_TOP_LEFT] = (struct rect_spec){
		l->bw, 0, l->grip_w + l->bw, l->bw,
		LAB_NODE_GRIP_LEFT };
	specs[HRECT_BORDER_TOP_CENTER] = (struct rect_spec){
		l->handle_x, 0, l->handle_w, l->bw,
		LAB_NODE_HANDLE };
	specs[HRECT_BORDER_TOP_RIGHT] = (struct rect_spec){
		l->grip_right_x - l->bw, 0, l->grip_w + l->bw, l->bw,
		LAB_NODE_GRIP_RIGHT };
	specs[HRECT_SEPARATOR_LEFT] = (struct rect_spec){
		l->bw + l->grip_w, l->content_y, l->bw, l->content_h,
		LAB_NODE_GRIP_LEFT };
	specs[HRECT_SEPARATOR_RIGHT] = (struct rect_spec){
		l->grip_right_x - l->bw, l->content_y, l->bw, l->content_h,
		LAB_NODE_GRIP_RIGHT };
}

static void
compute_elem_geometry(const struct handle_layout *l,
	struct elem_geometry elems[SSD_HANDLE_ELEMENT_COUNT])
{
	elems[SSD_HANDLE_ELEMENT_GRIP_LEFT] = (struct elem_geometry){
		l->grip_left_x, l->content_y, l->grip_w, l->content_h };
	elems[SSD_HANDLE_ELEMENT_CENTER] = (struct elem_geometry){
		l->handle_x, l->content_y, l->handle_w, l->content_h };
	elems[SSD_HANDLE_ELEMENT_GRIP_RIGHT] = (struct elem_geometry){
		l->grip_right_x, l->content_y, l->grip_w, l->content_h };
}

static struct wlr_scene_buffer *
create_handle_buffer(struct wlr_scene_tree *parent,
	struct wlr_buffer *buf, int x, int y, int w, int h)
{
	struct wlr_scene_buffer *sbuf =
		lab_wlr_scene_buffer_create(parent, buf);
	wlr_scene_node_set_position(&sbuf->node, x, y);
	wlr_scene_buffer_set_dest_size(sbuf, w, h);
	if (wlr_renderer_is_pixman(server.renderer)) {
		wlr_scene_buffer_set_filter_mode(
			sbuf, WLR_SCALE_FILTER_NEAREST);
	}
	return sbuf;
}

static void
create_inset_rects(struct wlr_scene_tree *parent,
	struct ssd_handle_subtree *subtree, int idx,
	int x, int y, int w, int h)
{
	/* Top inset (dark) */
	subtree->inset_top[idx] = lab_wlr_scene_rect_create(
		parent, w, INSET_LINE_WIDTH, inset_dark);
	wlr_scene_node_set_position(
		&subtree->inset_top[idx]->node, x, y);
	wlr_scene_node_set_enabled(
		&subtree->inset_top[idx]->node, false);

	/* Left inset (dark) */
	subtree->inset_left[idx] = lab_wlr_scene_rect_create(
		parent, INSET_LINE_WIDTH, h, inset_dark);
	wlr_scene_node_set_position(
		&subtree->inset_left[idx]->node, x, y);
	wlr_scene_node_set_enabled(
		&subtree->inset_left[idx]->node, false);

	/* Bottom inset (light highlight) */
	subtree->inset_bottom[idx] = lab_wlr_scene_rect_create(
		parent, w, INSET_LINE_WIDTH, inset_light);
	wlr_scene_node_set_position(
		&subtree->inset_bottom[idx]->node, x, y + h - INSET_LINE_WIDTH);
	wlr_scene_node_set_enabled(
		&subtree->inset_bottom[idx]->node, false);

	/* Right inset (light highlight) */
	subtree->inset_right[idx] = lab_wlr_scene_rect_create(
		parent, INSET_LINE_WIDTH, h, inset_light);
	wlr_scene_node_set_position(
		&subtree->inset_right[idx]->node,
		x + w - INSET_LINE_WIDTH, y);
	wlr_scene_node_set_enabled(
		&subtree->inset_right[idx]->node, false);
}

void
ssd_handle_create(struct ssd *ssd)
{
	assert(ssd);
	struct view *view = ssd->view;
	struct theme *theme = rc.theme;

	if (theme->handle_width <= 0) {
		ssd->handle.tree = NULL;
		return;
	}

	int width = view->current.width;
	struct handle_layout l = compute_layout(theme, width);

	ssd->handle.tree = lab_wlr_scene_tree_create(ssd->tree);
	wlr_scene_node_set_position(&ssd->handle.tree->node,
		-theme->border_width,
		view_effective_height(view, /* use_pending */ false));

	/* Initialize element states */
	for (int i = 0; i < SSD_HANDLE_ELEMENT_COUNT; i++) {
		ssd->handle.element_states[i] = SSD_HANDLE_STATE_NORMAL;
	}

	enum ssd_active_state active;
	FOR_EACH_ACTIVE_STATE(active) {
		struct ssd_handle_subtree *subtree =
			&ssd->handle.subtrees[active];
		subtree->tree = lab_wlr_scene_tree_create(ssd->handle.tree);
		struct wlr_scene_tree *parent = subtree->tree;
		wlr_scene_node_set_enabled(&parent->node, active);
		float *border_color = theme->window[active].border_color;

		/* Border and separator rects */
		struct rect_spec rspecs[HRECT_COUNT];
		compute_rect_specs(&l, rspecs);
		for (int i = 0; i < HRECT_COUNT; i++) {
			subtree->rects[i] = lab_wlr_scene_rect_create(
				parent, rspecs[i].w, rspecs[i].h,
				border_color);
			wlr_scene_node_set_position(
				&subtree->rects[i]->node,
				rspecs[i].x, rspecs[i].y);
			node_descriptor_create(&subtree->rects[i]->node,
				rspecs[i].desc_type, view, NULL);
		}

		/* Per-element geometry (shared by textures, overlays, insets) */
		struct elem_geometry elems[SSD_HANDLE_ELEMENT_COUNT];
		compute_elem_geometry(&l, elems);
		struct wlr_buffer *bufs[SSD_HANDLE_ELEMENT_COUNT] = {
			&theme->window[active].grip_fill->base,
			&theme->window[active].handle_fill->base,
			&theme->window[active].grip_fill->base,
		};

		for (int i = 0; i < SSD_HANDLE_ELEMENT_COUNT; i++) {
			/* Texture buffer (1px wide, stretched to fill) */
			subtree->textures[i] = create_handle_buffer(
				parent, bufs[i], elems[i].x, elems[i].y,
				elems[i].w, elems[i].h);
			node_descriptor_create(
				&subtree->textures[i]->node,
				elem_node_types[i], view, NULL);

			/* Overlay rect for hover/pressed dimming */
			subtree->overlay[i] = lab_wlr_scene_rect_create(
				parent, elems[i].w, elems[i].h,
				overlay_none);
			wlr_scene_node_set_position(
				&subtree->overlay[i]->node,
				elems[i].x, elems[i].y);
			wlr_scene_node_set_enabled(
				&subtree->overlay[i]->node, false);
			node_descriptor_create(
				&subtree->overlay[i]->node,
				elem_node_types[i], view, NULL);

			/* Inset shadow rects for pressed/dragging state */
			create_inset_rects(parent, subtree, i,
				elems[i].x, elems[i].y,
				elems[i].w, elems[i].h);
		}
	}

	if (view->maximized == VIEW_AXIS_BOTH) {
		wlr_scene_node_set_enabled(
			&ssd->handle.tree->node, false);
	}
}

void
ssd_handle_update(struct ssd *ssd)
{
	assert(ssd);

	if (!ssd->handle.tree) {
		return;
	}

	struct view *view = ssd->view;
	struct theme *theme = rc.theme;

	bool should_show = view_handle_visible(view)
		&& view->maximized != VIEW_AXIS_BOTH;

	if (!should_show) {
		if (ssd->handle.tree->node.enabled) {
			wlr_scene_node_set_enabled(
				&ssd->handle.tree->node, false);
		}
		return;
	} else if (!ssd->handle.tree->node.enabled) {
		wlr_scene_node_set_enabled(
			&ssd->handle.tree->node, true);
	}

	int width = view->current.width;
	int height = view_effective_height(view, /* use_pending */ false);
	struct handle_layout l = compute_layout(theme, width);

	/* Update position: handle sits below the client area */
	wlr_scene_node_set_position(&ssd->handle.tree->node,
		-theme->border_width, height);

	enum ssd_active_state active;
	FOR_EACH_ACTIVE_STATE(active) {
		struct ssd_handle_subtree *subtree =
			&ssd->handle.subtrees[active];

		float *border_color = theme->window[active].border_color;

		/* Update border and separator rects */
		struct rect_spec rspecs[HRECT_COUNT];
		compute_rect_specs(&l, rspecs);
		for (int i = 0; i < HRECT_COUNT; i++) {
			wlr_scene_rect_set_size(subtree->rects[i],
				rspecs[i].w, rspecs[i].h);
			wlr_scene_node_set_position(
				&subtree->rects[i]->node,
				rspecs[i].x, rspecs[i].y);
			wlr_scene_rect_set_color(subtree->rects[i],
				border_color);
		}

		/* Update textures, overlays, and insets per element */
		struct elem_geometry elems[SSD_HANDLE_ELEMENT_COUNT];
		compute_elem_geometry(&l, elems);

		for (int i = 0; i < SSD_HANDLE_ELEMENT_COUNT; i++) {
			int ex = elems[i].x;
			int ey = elems[i].y;
			int ew = elems[i].w;
			int eh = elems[i].h;

			wlr_scene_node_set_position(
				&subtree->textures[i]->node, ex, ey);
			wlr_scene_buffer_set_dest_size(
				subtree->textures[i], ew, eh);

			wlr_scene_rect_set_size(
				subtree->overlay[i], ew, eh);
			wlr_scene_node_set_position(
				&subtree->overlay[i]->node, ex, ey);

			wlr_scene_rect_set_size(
				subtree->inset_top[i],
				ew, INSET_LINE_WIDTH);
			wlr_scene_node_set_position(
				&subtree->inset_top[i]->node, ex, ey);

			wlr_scene_rect_set_size(
				subtree->inset_left[i],
				INSET_LINE_WIDTH, eh);
			wlr_scene_node_set_position(
				&subtree->inset_left[i]->node, ex, ey);

			wlr_scene_rect_set_size(
				subtree->inset_bottom[i],
				ew, INSET_LINE_WIDTH);
			wlr_scene_node_set_position(
				&subtree->inset_bottom[i]->node,
				ex, ey + eh - INSET_LINE_WIDTH);

			wlr_scene_rect_set_size(
				subtree->inset_right[i],
				INSET_LINE_WIDTH, eh);
			wlr_scene_node_set_position(
				&subtree->inset_right[i]->node,
				ex + ew - INSET_LINE_WIDTH, ey);
		}
	}
}

void
ssd_handle_destroy(struct ssd *ssd)
{
	assert(ssd);

	if (!ssd->handle.tree) {
		return;
	}

	wlr_scene_node_destroy(&ssd->handle.tree->node);
	ssd->handle = (struct ssd_handle_scene){0};
}

/*
 * Update the visual state of a single handle/grip element.
 * This toggles the overlay and inset shadow rects on both active
 * and inactive subtrees (only the enabled one is visible).
 */
void
ssd_handle_set_element_state(struct ssd *ssd, int element,
	enum ssd_handle_state state)
{
	if (!ssd || !ssd->handle.tree || element < 0
			|| element >= SSD_HANDLE_ELEMENT_COUNT) {
		return;
	}

	if (ssd->handle.element_states[element] == state) {
		return;
	}
	ssd->handle.element_states[element] = state;

	enum ssd_active_state active;
	FOR_EACH_ACTIVE_STATE(active) {
		struct ssd_handle_subtree *subtree =
			&ssd->handle.subtrees[active];
		struct wlr_scene_rect *overlay = subtree->overlay[element];

		bool show_overlay = false;
		bool show_insets = false;

		switch (state) {
		case SSD_HANDLE_STATE_NORMAL:
			break;
		case SSD_HANDLE_STATE_HOVER:
			wlr_scene_rect_set_color(overlay, overlay_hover);
			show_overlay = true;
			break;
		case SSD_HANDLE_STATE_PRESSED:
			wlr_scene_rect_set_color(overlay, overlay_pressed);
			show_overlay = true;
			show_insets = true;
			break;
		case SSD_HANDLE_STATE_DRAGGING:
			wlr_scene_rect_set_color(overlay, overlay_hover);
			show_overlay = true;
			show_insets = true;
			break;
		}

		wlr_scene_node_set_enabled(&overlay->node, show_overlay);
		wlr_scene_node_set_enabled(
			&subtree->inset_top[element]->node, show_insets);
		wlr_scene_node_set_enabled(
			&subtree->inset_left[element]->node, show_insets);
		wlr_scene_node_set_enabled(
			&subtree->inset_bottom[element]->node, show_insets);
		wlr_scene_node_set_enabled(
			&subtree->inset_right[element]->node, show_insets);
	}
}

void
ssd_handle_clear_all_states(struct ssd *ssd)
{
	if (!ssd || !ssd->handle.tree) {
		return;
	}
	for (int i = 0; i < SSD_HANDLE_ELEMENT_COUNT; i++) {
		ssd_handle_set_element_state(ssd, i,
			SSD_HANDLE_STATE_NORMAL);
	}
}

/*
 * Determine which handle element (if any) a scene node belongs to,
 * and resolve the owning view/SSD.
 *
 * Returns the element index (SSD_HANDLE_ELEMENT_*) or -1 if the
 * node is not part of any handle.  If found, *out_ssd is set to
 * the owning SSD.
 */
static int
handle_element_from_node(struct wlr_scene_node *node, struct ssd **out_ssd)
{
	*out_ssd = NULL;
	if (!node) {
		return -1;
	}

	/*
	 * Walk up the node tree to find a node_descriptor with
	 * a handle/grip type.
	 */
	struct wlr_scene_node *n = node;
	int element = -1;
	while (n) {
		if (n->data) {
			struct node_descriptor *desc = n->data;
			switch (desc->type) {
			case LAB_NODE_GRIP_LEFT:
				element = SSD_HANDLE_ELEMENT_GRIP_LEFT;
				break;
			case LAB_NODE_HANDLE:
				/*
				 * Center handle is identified for correct cursor
				 * shape but visual hover/pressed effects are
				 * intentionally limited to corner grips only.
				 */
				element = SSD_HANDLE_ELEMENT_CENTER;
				break;
			case LAB_NODE_GRIP_RIGHT:
				element = SSD_HANDLE_ELEMENT_GRIP_RIGHT;
				break;
			default:
				break;
			}
			if (element >= 0) {
				struct view *view = desc->view;
				if (view && view->ssd
						&& view->ssd->handle.tree) {
					*out_ssd = view->ssd;
				}
				return element;
			}
		}
		n = n->parent ? &n->parent->node : NULL;
	}
	return -1;
}

/*
 * Update hover state based on the node under the cursor.
 * Uses global server.hovered_handle_ssd / hovered_handle_element
 * to track state across views (mirrors ssd_update_hovered_button
 * pattern).
 *
 * Called from cursor_update_common() in cursor.c.
 */
void
ssd_update_hovered_handle(struct wlr_scene_node *node)
{
	struct ssd *new_ssd = NULL;
	int new_element = -1;
	int raw_element;

	raw_element = handle_element_from_node(node, &new_ssd);

	if (new_ssd && raw_element >= 0) {
		/*
		 * Visual hover/pressed effects apply to corner grips
		 * only.  The center handle still changes the cursor
		 * shape via node_type_to_edges() but receives no
		 * dimming overlay.
		 */
		if (raw_element != SSD_HANDLE_ELEMENT_CENTER) {
			new_element = raw_element;
		}
	}

	struct ssd *old_ssd = server.hovered_handle_ssd;
	int old_element = server.hovered_handle_element;

	/* Same element on same SSD -- nothing to do */
	if (old_ssd == new_ssd && old_element == new_element) {
		return;
	}

	/* Clear hover on old element */
	if (old_ssd && old_ssd->handle.tree && old_element >= 0) {
		if (old_ssd->handle.element_states[old_element]
				== SSD_HANDLE_STATE_HOVER) {
			ssd_handle_set_element_state(old_ssd,
				old_element, SSD_HANDLE_STATE_NORMAL);
		}
	}

	/* Set hover on new element */
	if (new_ssd && new_element >= 0) {
		if (new_ssd->handle.element_states[new_element]
				== SSD_HANDLE_STATE_NORMAL) {
			ssd_handle_set_element_state(new_ssd,
				new_element, SSD_HANDLE_STATE_HOVER);
		}
	}

	server.hovered_handle_ssd = new_ssd;
	server.hovered_handle_element = new_element;
}
