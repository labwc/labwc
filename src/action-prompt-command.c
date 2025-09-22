// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "action-prompt-command.h"
#include <stdio.h>
#include <wlr/util/log.h>
#include "action.h"
#include "common/buf.h"
#include "labwc.h" /* for gettext */
#include "theme.h"

enum {
	LAB_PROMPT_NONE = 0,
	LAB_PROMPT_MESSAGE,
	LAB_PROMPT_NO,
	LAB_PROMPT_YES,
	LAB_PROMPT_BG_COL,
	LAB_PROMPT_TEXT_COL,

	LAB_PROMPT_COUNT
};

typedef void field_conversion_type(struct buf *buf, struct action *action,
		struct theme *theme);

struct field_converter {
	const char fmt_char;
	field_conversion_type *fn;
};

/* %m */
static void
set_message(struct buf *buf, struct action *action, struct theme *theme)
{
	buf_add(buf, action_get_str(action, "message.prompt", "Choose wisely"));
}

/* %n */
static void
set_no(struct buf *buf, struct action *action, struct theme *theme)
{
	buf_add(buf, _("No"));
}

/* %y */
static void
set_yes(struct buf *buf, struct action *action, struct theme *theme)
{
	buf_add(buf, _("Yes"));
}

/* %b */
static void
set_bg_col(struct buf *buf, struct action *action, struct theme *theme)
{
	buf_add_hex_color(buf, theme->osd_bg_color);
}

/* %t */
static void
set_text_col(struct buf *buf, struct action *action, struct theme *theme)
{
	buf_add_hex_color(buf, theme->osd_label_text_color);
}

static const struct field_converter field_converter[LAB_PROMPT_COUNT] = {
	[LAB_PROMPT_MESSAGE]           = { 'm', set_message },
	[LAB_PROMPT_NO]                = { 'n', set_no },
	[LAB_PROMPT_YES]               = { 'y', set_yes },
	[LAB_PROMPT_BG_COL]            = { 'b', set_bg_col },
	[LAB_PROMPT_TEXT_COL]          = { 't', set_text_col },
};

void
action_prompt_command(struct buf *buf, const char *format,
		struct action *action, struct theme *theme)
{
	if (!format) {
		wlr_log(WLR_ERROR, "missing format");
		return;
	}

	for (const char *p = format; *p; p++) {
		/*
		 * If we're not on a conversion specifier (like %m) then just
		 * keep adding it to the buffer
		 */
		if (*p != '%') {
			buf_add_char(buf, *p);
			continue;
		}

		/* Process the %* conversion specifier */
		++p;

		bool found = false;
		for (unsigned char i = 0; i < LAB_PROMPT_COUNT; i++) {
			if (*p == field_converter[i].fmt_char) {
				field_converter[i].fn(buf, action, theme);
				found = true;
				break;
			}
		}
		if (!found) {
			wlr_log(WLR_ERROR,
				"invalid prompt command conversion specifier '%c'", *p);
		}
	}
}
