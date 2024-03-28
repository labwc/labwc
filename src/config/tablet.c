// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <linux/input-event-codes.h>
#include <stdint.h>
#include <strings.h>
#include <wlr/util/log.h>
#include "common/parse-double.h"
#include "config/tablet.h"
#include "config/rcxml.h"
#include "input/tablet_pad.h"

double
tablet_get_dbl_if_positive(const char *content, const char *name)
{
	double value = 0;
	set_double(content, &value);
	if (value < 0) {
		wlr_log(WLR_ERROR, "Invalid value for tablet area %s", name);
		return 0;
	}
	return value;
}

enum rotation
tablet_parse_rotation(int value)
{
	switch (value) {
	case 0:
		return LAB_ROTATE_NONE;
	case 90:
		return LAB_ROTATE_90;
	case 180:
		return LAB_ROTATE_180;
	case 270:
		return LAB_ROTATE_270;
	default:
		wlr_log(WLR_ERROR, "Invalid value for tablet rotation: %d", value);
		break;
	}
	return LAB_ROTATE_NONE;
}

uint32_t
tablet_button_from_str(const char *button)
{
	if (!strcasecmp(button, "Tip")) {
		return BTN_TOOL_PEN;
	} else if (!strcasecmp(button, "Stylus")) {
		return BTN_STYLUS;
	} else if (!strcasecmp(button, "Stylus2")) {
		return BTN_STYLUS2;
	} else if (!strcasecmp(button, "Stylus3")) {
		return BTN_STYLUS3;
	} else if (!strcasecmp(button, "Pad")) {
		return LAB_BTN_PAD;
	} else if (!strcasecmp(button, "Pad2")) {
		return LAB_BTN_PAD2;
	} else if (!strcasecmp(button, "Pad3")) {
		return LAB_BTN_PAD3;
	} else if (!strcasecmp(button, "Pad4")) {
		return LAB_BTN_PAD4;
	} else if (!strcasecmp(button, "Pad5")) {
		return LAB_BTN_PAD5;
	} else if (!strcasecmp(button, "Pad6")) {
		return LAB_BTN_PAD6;
	} else if (!strcasecmp(button, "Pad7")) {
		return LAB_BTN_PAD7;
	} else if (!strcasecmp(button, "Pad8")) {
		return LAB_BTN_PAD8;
	} else if (!strcasecmp(button, "Pad9")) {
		return LAB_BTN_PAD9;
	}
	wlr_log(WLR_ERROR, "Invalid value for tablet button: %s", button);
	return UINT32_MAX;
}

void
tablet_button_mapping_add(uint32_t from, uint32_t to)
{
	struct button_map_entry *entry;
	for (size_t i = 0; i < rc.tablet.button_map_count; i++) {
		entry = &rc.tablet.button_map[i];
		if (entry->from == from) {
			entry->to = to;
			wlr_log(WLR_INFO, "Overwriting button map for 0x%x with 0x%x", from, to);
			return;
		}
	}
	if (rc.tablet.button_map_count == BUTTON_MAP_MAX) {
		wlr_log(WLR_ERROR,
			"Failed to add button mapping: only supporting up to %u mappings",
			BUTTON_MAP_MAX);
		return;
	}
	wlr_log(WLR_INFO, "Adding button map for 0x%x with 0x%x", from, to);
	entry = &rc.tablet.button_map[rc.tablet.button_map_count];
	entry->from = from;
	entry->to = to;
	rc.tablet.button_map_count++;
}

void
tablet_load_default_button_mappings(void)
{
	rc.tablet.button_map_count = 0;
	tablet_button_mapping_add(BTN_TOOL_PEN, BTN_LEFT); /* Used for the pen tip */
	tablet_button_mapping_add(BTN_STYLUS, BTN_RIGHT);
	tablet_button_mapping_add(BTN_STYLUS2, BTN_MIDDLE);
}

uint32_t
tablet_get_mapped_button(uint32_t src_button)
{
	struct button_map_entry *map_entry;
	for (size_t i = 0; i < rc.tablet.button_map_count; i++) {
		map_entry = &rc.tablet.button_map[i];
		if (map_entry->from == src_button) {
			return map_entry->to;
		}
	}
	wlr_log(WLR_DEBUG, "no button map target for 0x%x", src_button);
	return 0;
}
