/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_INPUT_H
#define LABWC_INPUT_H

struct seat;

void input_handlers_init(struct seat *seat);
void input_handlers_finish(struct seat *seat);

#endif /* LABWC_INPUT_H */
