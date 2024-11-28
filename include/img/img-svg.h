/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_IMG_SVG_H
#define LABWC_IMG_SVG_H

#include <librsvg/rsvg.h>

struct lab_data_buffer;

RsvgHandle *img_svg_load(const char *filename);

struct lab_data_buffer *img_svg_render(RsvgHandle *svg, int w, int h,
	int padding, double scale);

#endif /* LABWC_IMG_SVG_H */
