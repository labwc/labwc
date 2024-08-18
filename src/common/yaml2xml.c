// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <yaml.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <wlr/util/log.h>
#include "common/buf.h"
#include "common/yaml2xml.h"

struct lab_yaml_event {
	yaml_event_type_t type;
	char scalar[256];
};

static bool process_mapping(yaml_parser_t *parser, struct buf *buf);
static bool process_sequence(yaml_parser_t *parser, struct buf *buf, char *key_name);

static bool
parse(yaml_parser_t *parser, struct lab_yaml_event *event)
{
	yaml_event_t yaml_event;
	if (!yaml_parser_parse(parser, &yaml_event)) {
		wlr_log(WLR_ERROR,
			"Conversion from YAML to XML failed at %ld:%ld: %s",
			parser->problem_mark.line + 1,
			parser->problem_mark.column,
			parser->problem);
		return false;
	}
	event->type = yaml_event.type;
	if (yaml_event.type == YAML_SCALAR_EVENT) {
		snprintf(event->scalar, sizeof(event->scalar), "%s",
			(char *)yaml_event.data.scalar.value);
	}
	yaml_event_delete(&yaml_event);
	return event;
}

static bool
process_value(yaml_parser_t *parser, struct buf *b,
		struct lab_yaml_event *event, char *key_name)
{
	const char *parent_name = NULL;

	switch (event->type) {
	case YAML_MAPPING_START_EVENT:
		buf_add_fmt(b, "<%s>", key_name);
		if (!process_mapping(parser, b)) {
			return false;
		}
		buf_add_fmt(b, "</%s>", key_name);
		break;
	case YAML_SEQUENCE_START_EVENT:
		/*
		 *     YAML                      XML
		 * fields: [x,y] -> <fields><field>x<field><field>y<field<fields>
		 */
		if (!strcasecmp(key_name, "fields")) {
			parent_name = "fields";
			key_name = "field";
		} else if (!strcasecmp(key_name, "regions")) {
			parent_name = "regions";
			key_name = "region";
		} else if (!strcasecmp(key_name, "windowRules")) {
			parent_name = "windowRules";
			key_name = "windowRule";
		} else if (!strcasecmp(key_name, "libinput")) {
			parent_name = "libinput";
			key_name = "device";
		} else if (!strcasecmp(key_name, "names")) {
			parent_name = "names";
			key_name = "name";
		} else if (!strcasecmp(key_name, "keybinds")) {
			key_name = "keybind";
		} else if (!strcasecmp(key_name, "mousebinds")) {
			key_name = "mousebind";
		} else if (!strcasecmp(key_name, "actions")) {
			key_name = "action";
		} else if (!strcasecmp(key_name, "fonts")) {
			key_name = "font";
		} else if (!strcasecmp(key_name, "contexts")) {
			key_name = "context";
		}
		if (parent_name) {
			buf_add_fmt(b, "<%s>", parent_name);
		}
		if (!process_sequence(parser, b, key_name)) {
			return false;
		}
		if (parent_name) {
			buf_add_fmt(b, "</%s>", parent_name);
		}
		break;
	case YAML_SCALAR_EVENT:
		/*
		 * To avoid duplicated keys, mousebind: {event: Press} is
		 * mapped to <mousebind action="Press">.
		 */
		if (!strcasecmp(key_name, "event")) {
			key_name = "action";
		}
		buf_add_fmt(b, "<%s>%s</%s>", key_name, event->scalar, key_name);
		break;
	default:
		break;
	}
	return true;
}

static bool
process_sequence(yaml_parser_t *parser, struct buf *b, char *key_name)
{
	while (1) {
		struct lab_yaml_event event;
		if (!parse(parser, &event)) {
			return false;
		}
		if (event.type == YAML_SEQUENCE_END_EVENT) {
			return true;
		}
		if (!process_value(parser, b, &event, key_name)) {
			return false;
		}
	}
}

static bool
process_mapping(yaml_parser_t *parser, struct buf *buf)
{
	while (1) {
		struct lab_yaml_event key_event, value_event;

		if (!parse(parser, &key_event)) {
			return false;
		}
		if (key_event.type == YAML_MAPPING_END_EVENT) {
			return true;
		}
		if (key_event.type != YAML_SCALAR_EVENT) {
			wlr_log(WLR_ERROR, "key must be scalar");
			return false;
		}
		if (!parse(parser, &value_event)) {
			return false;
		}
		if (!process_value(parser, buf, &value_event, key_event.scalar)) {
			return false;
		}
		continue;
	}
}

static bool
process_root(yaml_parser_t *parser, struct buf *b, const char *toplevel_name)
{
	struct lab_yaml_event event;
	if (!parse(parser, &event) || event.type != YAML_STREAM_START_EVENT) {
		return false;
	}
	if (!parse(parser, &event) || event.type != YAML_DOCUMENT_START_EVENT) {
		return false;
	}
	if (!parse(parser, &event) || event.type != YAML_MAPPING_START_EVENT) {
		wlr_log(WLR_ERROR, "mapping is expected for toplevel node");
		return false;
	}

	buf_add_fmt(b, "<%s>", toplevel_name);
	if (!process_mapping(parser, b)) {
		return false;
	}
	buf_add_fmt(b, "</%s>", toplevel_name);
	return true;
}

struct buf
yaml_to_xml(FILE *stream, const char *toplevel_name)
{
	struct buf b = BUF_INIT;
	yaml_parser_t parser;
	yaml_parser_initialize(&parser);
	yaml_parser_set_input_file(&parser, stream);

	bool success = process_root(&parser, &b, toplevel_name);
	if (success) {
		wlr_log(WLR_DEBUG, "XML converted from YAML: %s", b.data);
	} else {
		buf_reset(&b);
	}
	yaml_parser_delete(&parser);
	return b;
}
