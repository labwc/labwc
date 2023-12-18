/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_BUTTON_XBM_H
#define LABWC_BUTTON_XBM_H

struct lab_data_buffer;

/**
 * button_xbm_from_bitmap() - create button from monochrome bitmap
 * @bitmap: bitmap data array in hexadecimal xbm format
 * @buffer: cairo-surface-buffer to create
 * @rgba: color
 *
 * Example bitmap: char button[6] = { 0x3f, 0x3f, 0x21, 0x21, 0x21, 0x3f };
 */
void button_xbm_from_bitmap(const char *bitmap, struct lab_data_buffer **buffer,
	float *rgba);

/* button_xbm_load - Convert xbm file to buffer with cairo surface */
void button_xbm_load(const char *button_name, struct lab_data_buffer **buffer,
	float *rgba);

#endif /* LABWC_BUTTON_XBM_H */
