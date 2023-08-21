/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_BUTTON_SVG_H
#define LABWC_BUTTON_SVG_H

struct lab_data_buffer;

void button_svg_load(const char *button_name, struct lab_data_buffer **buffer,
	int size);

#endif /* LABWC_BUTTON_SVG_H */
