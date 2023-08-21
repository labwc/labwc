/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_BUTTON_XBM_H
#define LABWC_BUTTON_XBM_H

struct lab_data_buffer;

/* button_xbm_load - Convert xbm file to buffer with cairo surface */
void button_xbm_load(const char *button_name, struct lab_data_buffer **buffer,
	char *fallback_button, float *rgba);

#endif /* LABWC_BUTTON_XBM_H */
