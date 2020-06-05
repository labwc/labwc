#ifndef RCXML_H
#define RCXML_H

#include <stdio.h>
#include <stdbool.h>

struct rcxml {
	bool client_side_decorations;
};

extern struct rcxml rc;

void rcxml_init(struct rcxml *rc);
void rcxml_read(const char *filename);
void rcxml_set_verbose(void);

#endif /* RCXML_H */
