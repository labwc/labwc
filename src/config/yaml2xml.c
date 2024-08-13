// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <yaml.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wlr/util/log.h>
#include "common/buf.h"
#include "common/mem.h"
#include "config/yaml2xml.h"

static struct element *elements;
static int nr_elements, alloc_elements;

struct element {
	char name[64];
};

static struct element *
push(const char *name)
{
	if (nr_elements == alloc_elements) {
		alloc_elements = (alloc_elements + 16) * 2;
		elements = xrealloc(elements,
			alloc_elements * sizeof(struct element));
	}
	struct element *element = elements + nr_elements;
	memset(element, 0, sizeof(*element));
	nr_elements++;
	strncpy(element->name, name, sizeof(element->name));
	element->name[sizeof(element->name) - 1] = '\0';
	return element;
}

static void
pop(char *buffer, size_t len)
{
	strncpy(buffer, elements[nr_elements-1].name, len);
	nr_elements--;
}

static void
process_one_event(struct buf *buffer, yaml_event_t *event)
{
	static bool in_scalar;

	switch (event->type) {
	case YAML_NO_EVENT:
	case YAML_ALIAS_EVENT:
	case YAML_MAPPING_START_EVENT:
	case YAML_MAPPING_END_EVENT:
	case YAML_STREAM_START_EVENT:
	case YAML_STREAM_END_EVENT:
		break;
	case YAML_DOCUMENT_START_EVENT:
		buf_add(buffer, "<?xml version=\"1.0\"?>\n");
		buf_add(buffer, "<labwc_config>\n");
		break;
	case YAML_DOCUMENT_END_EVENT:
		buf_add(buffer, "</labwc_config>\n");
		break;
	case YAML_SCALAR_EVENT:
		if (in_scalar) {
			char element[64] = { 0 };
			pop(element, sizeof(element));
			buf_add(buffer, (char *)event->data.scalar.value);
			buf_add(buffer, "</");
			buf_add(buffer, element);
			buf_add(buffer, ">\n");
			in_scalar = false;
		} else {
			char *value = (char *)event->data.scalar.value;
			buf_add(buffer, "<");
			buf_add(buffer, value);
			buf_add(buffer, ">");
			push(value);
			in_scalar = true;
		}
		break;
	case YAML_SEQUENCE_START_EVENT:
		assert(in_scalar);
		buf_add(buffer, "\n");
		in_scalar = false;
		break;
	case YAML_SEQUENCE_END_EVENT:
		char element[64] = { 0 };
		pop(element, sizeof(element));
		buf_add(buffer, "</");
		buf_add(buffer, element);
		buf_add(buffer, ">\n");
		break;
	}
}

bool
yaml_to_xml(struct buf *buffer, const char *filename)
{
	bool ret = false;
	FILE *f = fopen(filename, "r");
	if (!f) {
		return false;
	}
	yaml_parser_t parser;
	yaml_parser_initialize(&parser);
	yaml_parser_set_input_file(&parser, f);

	yaml_event_type_t event_type = 0;
	while (event_type != YAML_STREAM_END_EVENT) {
		yaml_event_t event;
		int success = yaml_parser_parse(&parser, &event);
		if (!success) {
			wlr_log(WLR_ERROR, "yaml parse: %s", parser.problem);
			yaml_parser_delete(&parser);
			goto out;
		}
		process_one_event(buffer, &event);
		event_type = event.type;
		yaml_event_delete(&event);
	}
	yaml_parser_delete(&parser);
	free(elements);
	nr_elements = 0;
	alloc_elements = 0;
	ret = true;
out:
	fclose(f);
	return ret;
}
