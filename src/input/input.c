// SPDX-License-Identifier: GPL-2.0-only
#include "input/cursor.h"
#include "input/input.h"
#include "input/keyboard.h"

void
input_handlers_init(struct seat *seat)
{
	cursor_init(seat);
	keyboard_group_init(seat);
}

void
input_handlers_finish(struct seat *seat)
{
	cursor_finish(seat);
	keyboard_group_finish(seat);
}
