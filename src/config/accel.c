// SPDX-License-Identifier: GPL-2.0-only
#include "config/libinput.h"
#include <glib.h>
#include <math.h>
#include <strings.h>
#include <wlr/config.h>
#include <wlr/util/log.h>
#include "common/parse-double.h"
#include "common/xml.h"

static bool
parse_points(const char *content, struct lab_accel_curve *curve)
{
	char **tokens = g_strsplit_set(content, " \t\r\n", -1);
	bool valid = true;
	for (size_t i = 0; tokens[i]; ++i) {
		if (!*tokens[i]) {
			continue;
		}
		double point = 0;
		if (curve->nr_points == LAB_ACCEL_CURVE_MAX_POINTS
				|| !set_double(tokens[i], &point)
				|| !isfinite(point) || point < 0) {
			valid = false;
			break;
		}
		curve->points[curve->nr_points++] = point;
	}
	g_strfreev(tokens);
	return valid && curve->nr_points >= 2;
}

bool
libinput_curve_parse(xmlNode *node, struct lab_accel_curve *curve)
{
	struct lab_accel_curve parsed = {0};
	bool have_step = false;
	bool have_points = false;
	bool valid = true;
	/* Read named children after attribute expansion, never parent content. */
	for (xmlNode *child = node->children; child; child = child->next) {
		if (child->type == XML_COMMENT_NODE) {
			continue;
		}
		char *content = (char *)xmlNodeGetContent(child);
		if (child->type == XML_TEXT_NODE) {
			valid &= strspn(content, " \t\r\n") == strlen(content);
		} else if (child->type != XML_ELEMENT_NODE
				|| !lab_xml_node_is_leaf(child)) {
			valid = false;
		} else if (!strcasecmp((char *)child->name, "step") && !have_step) {
			have_step = true;
			valid &= set_double(g_strstrip(content), &parsed.step)
				&& isfinite(parsed.step) && parsed.step > 0;
		} else if (!strcasecmp((char *)child->name, "points") && !have_points) {
			have_points = true;
			valid &= parse_points(content, &parsed);
		} else {
			valid = false;
		}
		xmlFree((xmlChar *)content);
	}
	if (!valid || !have_step || !have_points) {
		wlr_log(WLR_ERROR, "<%s> requires a positive finite step and "
			"2 to %d finite non-negative output-speed points",
			node->name, LAB_ACCEL_CURVE_MAX_POINTS);
		return false;
	}
	*curve = parsed;
	return true;
}

#if WLR_HAS_LIBINPUT_BACKEND
struct libinput_config_accel *
libinput_custom_accel_create(const struct libinput_category *category)
{
	if (category->accel_curve_invalid) {
		wlr_log(WLR_ERROR, "invalid curve; custom acceleration not applied");
		return NULL;
	}
	struct libinput_config_accel *config = libinput_config_accel_create(
		LIBINPUT_CONFIG_ACCEL_PROFILE_CUSTOM);
	if (!config) {
		wlr_log(WLR_ERROR, "could not create custom acceleration config");
		return NULL;
	}
	for (enum libinput_config_accel_type type = LIBINPUT_ACCEL_TYPE_FALLBACK;
			type <= LIBINPUT_ACCEL_TYPE_SCROLL; ++type) {
		const struct lab_accel_curve *curve = &category->accel_curves[type];
		if (!curve->nr_points) {
			continue;
		}
		enum libinput_config_status status = libinput_config_accel_set_points(
			config, type, curve->step, curve->nr_points, curve->points);
		if (status != LIBINPUT_CONFIG_STATUS_SUCCESS) {
			wlr_log(WLR_ERROR, "custom acceleration curve %d: %s", type,
				libinput_config_status_to_str(status));
			libinput_config_accel_destroy(config);
			return NULL;
		}
	}
	return config;
}
#endif
