/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_IMG_SVG_H
#define LABWC_IMG_SVG_H

struct lab_data_buffer;

void img_svg_load(const char *filename, struct lab_data_buffer **buffer,
	int size, float scale);

#endif /* LABWC_IMG_SVG_H */
