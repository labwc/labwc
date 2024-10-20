/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SET_H
#define LABWC_SET_H

#include <stdbool.h>
#include <stdint.h>

#define LAB_SET_MAX_SIZE 16

struct lab_set {
	uint32_t values[LAB_SET_MAX_SIZE];
	int size;
};

bool lab_set_contains(struct lab_set *set, uint32_t value);
void lab_set_add(struct lab_set *set, uint32_t value);
void lab_set_remove(struct lab_set *set, uint32_t value);

#endif /* LABWC_SET_H */
