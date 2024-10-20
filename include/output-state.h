/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_OUTPUT_STATE_H
#define LABWC_OUTPUT_STATE_H

#include <stdbool.h>

struct output;

void output_state_init(struct output *output);

bool output_state_commit(struct output *output);

#endif // LABWC_OUTPUT_STATE_H
