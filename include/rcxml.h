#ifndef RCXML_H
#define RCXML_H

#include <stdio.h>
#include <stdbool.h>

#include "buf.h"

struct rcxml {
	bool client_side_decorations;
};

extern struct rcxml rc;

void rcxml_init(struct rcxml *rc);
void rcxml_parse_xml(struct buf *b);
void rcxml_read(const char *filename);
void rcxml_set_verbose(void);

#endif /* RCXML_H */
