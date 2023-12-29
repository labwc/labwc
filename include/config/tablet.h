/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_TABLET_CONFIG_H
#define LABWC_TABLET_CONFIG_H

#include <stdint.h>

#define BUTTON_MAP_MAX 16
struct button_map_entry {
	uint32_t from;
	uint32_t to;
};

uint32_t tablet_button_from_str(const char *button);
uint32_t mouse_button_from_str(const char *button);
void tablet_button_mapping_add(uint32_t from, uint32_t to);
void tablet_load_default_button_mappings(void);

#endif /* LABWC_TABLET_CONFIG_H */
