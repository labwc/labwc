/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_TABLET_CONFIG_H
#define LABWC_TABLET_CONFIG_H

#include <stdint.h>

enum rotation {
	LAB_ROTATE_NONE = 0,
	LAB_ROTATE_90,
	LAB_ROTATE_180,
	LAB_ROTATE_270,
};

#define BUTTON_MAP_MAX 16
struct button_map_entry {
	uint32_t from;
	uint32_t to;
};

double tablet_get_dbl_if_positive(const char *content, const char *name);
enum rotation tablet_parse_rotation(int value);
uint32_t tablet_button_from_str(const char *button);
void tablet_button_mapping_add(uint32_t from, uint32_t to);
void tablet_load_default_button_mappings(void);

#endif /* LABWC_TABLET_CONFIG_H */
