/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_OUTPUT_STATE_H
#define LABWC_OUTPUT_STATE_H

#include <stdbool.h>

struct output;
struct wlr_output;

void output_state_init(struct output *output);

/* Forward port of removed functions */

bool wlr_output_test(struct wlr_output *wlr_output);

bool wlr_output_commit(struct wlr_output *wlr_output);

void wlr_output_enable(struct wlr_output *wlr_output, bool enabled);

void wlr_output_set_mode(struct wlr_output *wlr_output,
	struct wlr_output_mode *mode);

void wlr_output_set_custom_mode(struct wlr_output *wlr_output,
		int32_t width, int32_t height, int32_t refresh);

void wlr_output_set_scale(struct wlr_output *wlr_output, float scale);

void wlr_output_set_transform(struct wlr_output *wlr_output,
	enum wl_output_transform transform);

void wlr_output_enable_adaptive_sync(struct wlr_output *wlr_output,
	bool enabled);

#endif // LABWC_OUTPUT_STATE_H
