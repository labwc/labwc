/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_IMG_XBM_H
#define LABWC_IMG_XBM_H

struct lab_data_buffer;

/**
 * img_xbm_from_bitmap() - create button from monochrome bitmap
 * @bitmap: bitmap data array in hexadecimal xbm format
 * @buffer: cairo-surface-buffer to create
 * @rgba: color
 *
 * Example bitmap: char button[6] = { 0x3f, 0x3f, 0x21, 0x21, 0x21, 0x3f };
 */
void img_xbm_from_bitmap(const char *bitmap, struct lab_data_buffer **buffer,
	float *rgba);

/* img_xbm_load - Convert xbm file to buffer with cairo surface */
void img_xbm_load(const char *filename, struct lab_data_buffer **buffer,
	float *rgba);

#endif /* LABWC_IMG_XBM_H */
