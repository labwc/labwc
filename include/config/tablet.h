/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_TABLET_CONFIG_H
#define LABWC_TABLET_CONFIG_H

#include <stdint.h>
#include "config/types.h"

double tablet_get_dbl_if_positive(const char *content, const char *name);
enum lab_rotation tablet_parse_rotation(int value);
uint32_t tablet_button_from_str(const char *button);
void tablet_button_mapping_add(uint32_t from, uint32_t to);
void tablet_load_default_button_mappings(void);
uint32_t tablet_get_mapped_button(uint32_t src_button);

#endif /* LABWC_TABLET_CONFIG_H */
