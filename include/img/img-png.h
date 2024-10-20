/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_IMG_PNG_H
#define LABWC_IMG_PNG_H

struct lab_data_buffer;

void img_png_load(const char *filename, struct lab_data_buffer **buffer,
	int size, float scale);

#endif /* LABWC_IMG_PNG_H */
