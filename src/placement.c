// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include "common/macros.h"
#include "common/mem.h"
#include "labwc.h"
#include "placement.h"
#include "ssd.h"
#include "view.h"

#define overlap_bitmap_index(bmp, i, j) \
	(bmp)->grid[i * ((bmp)->nr_cols - 1) + j]

struct overlap_bitmap {
	int nr_rows;
	int nr_cols;
	int *rows;
	int *cols;
	int *grid;
};

static int
compare_ints(const void *a, const void *b)
{
	return *(const int *)a - *(const int *)b;
}

static void
destroy_bitmap(struct overlap_bitmap *bmp)
{
	assert(bmp);

	zfree(bmp->rows);
	zfree(bmp->cols);
	zfree(bmp->grid);

	bmp->nr_rows = 0;
	bmp->nr_cols = 0;
}

/* Count the number of views on view->output, excluding *view itself */
static int
count_views(struct view *view)
{
	assert(view);

	struct server *server = view->server;
	assert(server);

	struct output *output = view->output;
	if (!output_is_usable(output)) {
		return 0;
	}

	int nviews = 0;

	struct view *v;
	for_each_view(v, &server->views, LAB_VIEW_CRITERIA_CURRENT_WORKSPACE) {
		/* Ignore the target view or anything on a different output */
		if (v == view || v->output != output) {
			continue;
		}

		nviews++;
	}

	return nviews;
}

/* Sort and de-deplicate a list of points that define a 1-D grid */
static int
order_grid(int *edges, int nedges)
{
	/* Sort grid edges */
	qsort(edges, nedges, sizeof(int), compare_ints);

	/* Skip over non-unique edges, counting the unique ones */
	/* This is taken almost verbatim from Openbox. */
	int i = 0;
	int j = 0;

	while (j < nedges) {
		int last = edges[j++];
		edges[i++] = last;
		while (j < nedges && edges[j] == last) {
			++j;
		}
	}

	return i;
}

/*
 * Construct an irregular grid that divides the usable area of view->output
 * by extending the edges of every view on the output (except for *view itself)
 * to infinity. The resulting grid will consist of rectangular intervals that
 * are either completely uncovered by any view, or entirely covered.
 * Furthermore, when any view intersects any interval on the grid, that view
 * overlaps the whole interval: no view ever partially intersects any interval.
 */
static void
build_grid(struct overlap_bitmap *bmp, struct view *view)
{
	assert(bmp);
	assert(view);

	struct server *server = view->server;
	assert(server);

	/* Always start with a fresh bitmap */
	destroy_bitmap(bmp);

	struct output *output = view->output;
	if (!output_is_usable(output)) {
		return;
	}

	int nviews = count_views(view);
	if (nviews < 1) {
		return;
	}

	/* Number of rows/columns is bounded by two per view plus screen edges */
	int max_rc = 2 * nviews + 2;

	bmp->rows = xzalloc(max_rc * sizeof(int));
	bmp->cols = xzalloc(max_rc * sizeof(int));
	if (!bmp->rows || !bmp->cols) {
		destroy_bitmap(bmp);
		return;
	}

	/* First edges of grid are start of usable area of output */
	struct wlr_box usable = output_usable_area_in_layout_coords(output);
	int usable_right = usable.x + usable.width;
	int usable_bottom = usable.y + usable.height;

	bmp->cols[0] = usable.x;
	bmp->rows[0] = usable.y;

	bmp->cols[1] = usable_right;
	bmp->rows[1] = usable_bottom;

	int nr_rows = 2;
	int nr_cols = 2;

	struct view *v;
	for_each_view(v, &server->views, LAB_VIEW_CRITERIA_CURRENT_WORKSPACE) {
		if (v == view || v->output != output) {
			continue;
		}

		struct border margin = ssd_get_margin(v->ssd);
		int x = v->pending.x - margin.left;
		int y = v->pending.y - margin.top;

		/* Add a column if the left view edge is in the usable region */
		if (x > usable.x && x < usable_right) {
			assert(nr_cols < max_rc);
			bmp->cols[nr_cols++] = x;
		}

		/* Add a row if the top view edge is in the usable region */
		if (y > usable.y && y < usable_bottom) {
			assert(nr_rows < max_rc);
			bmp->rows[nr_rows++] = y;
		}

		x = v->pending.x + margin.right + v->pending.width;
		y = v->pending.y + margin.bottom
			+ view_effective_height(v, /* use_pending */ true);

		/* Add a column if the right view edge is in the usable region */
		if (x > usable.x && x < usable_right) {
			assert(nr_cols < max_rc);
			bmp->cols[nr_cols++] = x;
		}

		/* Add a row if the bottom view edge is in the usable region */
		if (y > usable.y && y < usable_bottom) {
			assert(nr_rows < max_rc);
			bmp->rows[nr_rows++] = y;
		}
	}

	bmp->nr_rows = order_grid(bmp->rows, nr_rows);
	bmp->nr_cols = order_grid(bmp->cols, nr_cols);

	int grid_size = (bmp->nr_rows - 1) * (bmp->nr_cols - 1);

	bmp->grid = xzalloc(grid_size * sizeof(int));
	if (!bmp->grid) {
		destroy_bitmap(bmp);
		return;
	}
}

/*
 * Perform a rightmost binary search along a list of edges in a 1-D grid for
 * the maximum index j such that edges[j] <= val. The list of edges must be
 * sorted in increasing order.
 *
 * For a returned index j:
 *
 * - The index j == -1 implies that val < edges[0].
 * - An index 0 <= j < (nedges - 1) implies that edges[j] <= val < edges[j + 1].
 * - The index j == (nedges - 1) implies that edges[nedges - 1] <= val.
 */
static int
find_interval(int *edges, int nedges, double val)
{
	int l = 0;
	int r = nedges;

	while (l < r) {
		int m = (l + r) / 2;
		if (edges[m] > val) {
			r = m;
		} else {
			l = m + 1;
		}
	}

	return r - 1;
}

/*
 * Construct an overlap bitmap for the irregular grid, computed by
 * build_grid(), that spans view->output. The overlap bitmap maps
 * each interval to the number of views on the output (excluding *view)
 * that overlap that interval.
 */
static void
build_overlap(struct overlap_bitmap *bmp, struct view *view)
{
	assert(bmp);
	assert(view);

	struct server *server = view->server;
	assert(server);

	if (bmp->nr_rows < 1 || bmp->nr_cols < 1) {
		return;
	}

	struct output *output = view->output;
	if (!output_is_usable(output)) {
		return;
	}

	struct view *v;
	for_each_view(v, &server->views, LAB_VIEW_CRITERIA_CURRENT_WORKSPACE) {
		if (v == view || v->output != output) {
			continue;
		}

		/* Find boundaries of the window */
		struct border margin = ssd_get_margin(v->ssd);
		int lx = v->pending.x - margin.left;
		int ly = v->pending.y - margin.top;
		int hx = v->pending.x + margin.right + v->pending.width;
		int hy = v->pending.y + margin.bottom
			+ view_effective_height(v, /* use_pending */ true);

		/*
		 * Find the first and last row and column intervals spanned by
		 * this view. We want the left and top edges to fall in a
		 * half-open interval [low, high) but the right and bottom
		 * edges to fall in a half-open interval (low, high] to ensure
		 * that the results do not include intervals adjacent to the
		 * view. View edges are guaranteed by construction to fall
		 * exactly on the grid points, so we perturb the left and top
		 * edges by +0.5 units, and the right and bottom edges by -0.5
		 * units, to ensure that we are always searching in the
		 * interior of an interval.
		 */

		/* First row and column overlapping the view */
		int fc = find_interval(bmp->cols, bmp->nr_cols, lx + 0.5);
		int fr = find_interval(bmp->rows, bmp->nr_rows, ly + 0.5);

		/* Clip first row/column to start of usable grid */
		fc = MAX(fc, 0);
		fr = MAX(fr, 0);

		/* Last row and column overlapping the view */
		int lc = find_interval(bmp->cols, bmp->nr_cols, hx - 0.5);
		int lr = find_interval(bmp->rows, bmp->nr_rows, hy - 0.5);

		/*
		 * Increment the last indices to convert them to strict upper
		 * bounds, then clip them to the limits of the usable grid.
		 */
		lc = MIN(bmp->nr_cols - 1, lc + 1);
		lr = MIN(bmp->nr_rows - 1, lr + 1);

		/*
		 * Every interval in the region [fr, lr) x [fc, lc) is
		 * completely covered by the view. Increment the overlap
		 * counters these intervals to account for the view.
		 */
		for (int i = fr; i < lr; ++i) {
			for (int j = fc; j < lc; ++j) {
				overlap_bitmap_index(bmp, i, j) += 1;
			}
		}
	}
}

/*
 * Find the total overlap of an arbitrary region of a given width and height
 * with intervals in a pre-computed overlap bitmap. The starting interval for
 * the region is (i, j) in the bitmap grid. If the region is larger than
 * interval (i, j), neighboring regions will be considered width-wise rightward
 * (when right is true) or leftward (otherwise) and height-wise downward (when
 * down is true) or upward (otherwise).
 *
 * If the region would extend beyond the edges of the grid (i.e., beyond the
 * usable region of an output) in the prescribed directions, an overlap of
 * INT_MAX is returned. Otherwise, the overlap is the sum of the areas of each
 * interval covered by the region multiplied by its overlap count. For example,
 * an interval currently covered by three windows will be triply counted in the
 * overlap sum.
 */
static int
compute_overlap(struct overlap_bitmap *bmp, int i, int j,
		int width, int height, bool right, bool down, bool *single)
{
	/*
	 * The number of row or column intervals is one less than corresponding
	 * number of row or column grid points.
	 */
	int nri = bmp->nr_rows - 1;
	int nci = bmp->nr_cols - 1;

	int i_incr = down ? 1 : -1;
	int j_incr = right ? 1 : -1;

	int overlap = 0;
	int count = 0;

	/* Walk up or down along rows according to preference */
	for (int ii = i; ii >= 0 && ii < nri && height > 0; ii += i_incr) {
		/* Height of this row */
		int rh = bmp->rows[ii + 1] - bmp->rows[ii];

		/* Height of overlap between this row and test region */
		int mh = MAX(0, MIN(height, rh));

		/* Remaining height to consider for next row */
		height -= rh;

		/* Walk left or right along columns according to preference */
		int ww = width;
		for (int jj = j; jj >= 0 && jj < nci && ww > 0; jj += j_incr) {
			/* Width of this column */
			int cw = bmp->cols[jj + 1] - bmp->cols[jj];

			/* Width of overlap between this column and test region */
			int mw = MAX(0, MIN(ww, cw));

			/* Add overlap contribution for this interval */
			overlap += overlap_bitmap_index(bmp, ii, jj) * mh * mw;

			/* Count the number of overlapping intervals */
			count++;

			/* Remaining width to consider for next column */
			ww -= cw;
		}

		/*
		 * If there is width left to consider after walking columns,
		 * the region extends out of bounds and placement is invalid.
		 */
		if (ww > 0) {
			overlap = INT_MAX;
			break;
		}
	}

	/*
	 * If there is height left ot consider after walking rows, the region
	 * extends out of bounds and placement is invalid.
	 */
	if (height > 0) {
		overlap = INT_MAX;
	}

	/* Indicate whether overlap is confined to a single region */
	if (single) {
		*single = (count == 1);
	}

	return overlap;
}

/*
 * Find the placement of *view, with an expected width and height of
 * geometry->width and geometry->height, respectively, that will minimize
 * overlap with all other views. The values geometry->x and geometry->y will be
 * overwritten with the optimum placement.
 */
bool
placement_find_best(struct view *view, struct wlr_box *geometry)
{
	assert(view);

	struct server *server = view->server;
	assert(server);

	struct border margin = ssd_get_margin(view->ssd);

	struct output *output = view->output;
	if (!output_is_usable(output)) {
		return false;
	}

	/* Default placement is upper-left corner, respecting gaps */
	struct wlr_box usable = output_usable_area_in_layout_coords(output);
	geometry->x = usable.x + margin.left + rc.gap;
	geometry->y = usable.y + margin.top + rc.gap;

	/* Build the placement grid and overlap bitmap */
	struct overlap_bitmap bmp = { 0 };
	build_grid(&bmp, view);
	build_overlap(&bmp, view);

	/* Dimensions include gap along all edges to ensure proper separation */
	int height = geometry->height + margin.top + margin.bottom + 2 * rc.gap;
	int width = geometry->width + margin.left + margin.right + 2 * rc.gap;

	/*
	 * Overlap search identifies corners of the target region; view
	 * coordinates must by set in by the SSD margin and user gaps.
	 */
	int offset_x = margin.left + rc.gap;
	int offset_y = margin.top + rc.gap;

	int min_overlap = INT_MAX;

	int nri = bmp.nr_rows - 1;
	int nci = bmp.nr_cols - 1;

	/*
	 * Convolve the view region with the overlap grid to determine the
	 * total overlap of the view in all possible positions on the grid.
	 *
	 * When the view starts in a particular interval and is wider than the
	 * interval, it can extend either rightward (by placing the left edge
	 * of the view on the left edge of the interval) or leftward (by
	 * placing the right edge of the view on the right edge of the
	 * interval) into adjoining intervals. Likewise, when the view is wider
	 * than the interval in which it starts, it can extend either upward
	 * (by placing the bottom edge of the view on the bottom edge of the
	 * interval) or downward (by placing the top edge of the view on the
	 * top edge of the interval). All four possibilities produce different
	 * overlap characteristics and need to be checked independently.
	 *
	 * If the view is no larger than the interval in which it starts, there
	 * is no need to check multiple directions---the overlap will be the
	 * same regardless of where in the interval the window is placed.
	 *
	 * The interval (and, when the view spans more than one interval,
	 * directions in which it should extend) that produces the smallest
	 * overlap with other windows will determine the view placement.
	 */
	for (int i = 0; i < nri; ++i) {
		for (int j = 0; j < nci; ++j) {
			/*
			 * Search all directions, as a two-bit field, starting
			 * from interval (i, j).
			 */
			for (int ii = 0; ii < 4; ++ii) {
				/* Left/right is determined by first bit */
				bool rt = (ii & 0x1) == 0;
				/* Up/down is determined by second bit */
				bool dn = (ii & 0x2) == 0;

				/* Track whether overlap comes from single region */
				bool single = false;

				/* Compute overlap in specified direction */
				int overlap = compute_overlap(&bmp, i, j,
					width, height, rt, dn, &single);

				/* Move on if overlap isn't reduced */
				if (overlap >= min_overlap) {
					continue;
				}

				/* Place window in optimal direction */
				min_overlap = overlap;

				if (rt) {
					/* Extend window right from left edge */
					geometry->x = bmp.cols[j] + offset_x;
				} else {
					/* Extend window left from right edge */
					geometry->x =
						bmp.cols[j + 1] - width + offset_x;
				}

				if (dn) {
					/* Extend window down from top edge */
					geometry->y = bmp.rows[i] + offset_y;
				} else {
					/* Extend window up from bottom edge */
					geometry->y =
						bmp.rows[i + 1] - height + offset_y;
				}

				/* If there is no overlap, the search is done. */
				if (min_overlap <= 0) {
					goto final_placement;
				}

				/*
				 * Skip multi-directional searches when the
				 * view fits completely within one region.
				 */
				if (single) {
					break;
				}
			}
		}
	}

final_placement:
	destroy_bitmap(&bmp);
	return true;
}
