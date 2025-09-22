/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_ACTION_PROMPT_COMMAND_H
#define LABWC_ACTION_PROMPT_COMMAND_H

struct buf;
struct action;
struct theme;

void action_prompt_command(struct buf *buf, const char *format,
	struct action *action, struct theme *theme);

#endif /* LABWC_ACTION_PROMPT_COMMAND_H */
