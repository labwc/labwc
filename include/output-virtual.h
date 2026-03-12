/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_OUTPUT_VIRTUAL_H
#define LABWC_OUTPUT_VIRTUAL_H

struct server;
struct wlr_output;

void output_virtual_add(const char *output_name,
		struct wlr_output **store_wlr_output);
void output_virtual_remove(const char *output_name);
void output_virtual_update_fallback(void);

#endif
