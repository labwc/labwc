// SPDX-License-Identifier: GPL-2.0-only
#include "common/set.h"
#include <wlr/util/log.h>

bool
lab_set_contains(struct lab_set *set, uint32_t value)
{
	for (int i = 0; i < set->size; ++i) {
		if (set->values[i] == value) {
			return true;
		}
	}
	return false;
}

void
lab_set_add(struct lab_set *set, uint32_t value)
{
	if (lab_set_contains(set, value)) {
		return;
	}
	if (set->size >= LAB_SET_MAX_SIZE) {
		wlr_log(WLR_ERROR, "lab_set size exceeded");
		return;
	}
	set->values[set->size++] = value;
}

void
lab_set_remove(struct lab_set *set, uint32_t value)
{
	for (int i = 0; i < set->size; ++i) {
		if (set->values[i] == value) {
			--set->size;
			set->values[i] = set->values[set->size];
			return;
		}
	}
}
