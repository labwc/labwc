/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_YAML2XML_H
#define LABWC_YAML2XML_H
#include <common/buf.h>
#include <stdio.h>

struct buf yaml_to_xml(FILE *stream, const char *toplevel_name);

#endif /* LABWC_YAML2XML_H */
