/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_IMG_XBM_H
#define LABWC_IMG_XBM_H

struct lab_data_buffer;

struct lab_data_buffer *img_xbm_load_from_bitmap(const char *bitmap, float *rgba);

struct lab_data_buffer *img_xbm_load(const char *filename, float *rgba);

#endif /* LABWC_IMG_XBM_H */
