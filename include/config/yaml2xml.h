/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_YAML2XML_H
#define LABWC_YAML2XML_H
#include <stdbool.h>
#include <stdio.h>

struct buf;

bool yaml_to_xml(struct buf *buffer, const char *filename);

#endif /* LABWC_YAML2XML_H */
