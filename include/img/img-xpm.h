/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_IMG_XPM_H
#define LABWC_IMG_XPM_H

struct lab_data_buffer;

/**
 * img_xpm_load() - Convert xpm file to buffer with cairo surface
 * @filename: xpm file
 * @buffer: cairo-surface-buffer to create
 */
void img_xpm_load(const char *filename, struct lab_data_buffer **buffer);

#endif /* LABWC_IMG_XPM_H */
