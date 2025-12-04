/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_CONFIG_TYPES_H
#define LABWC_CONFIG_TYPES_H

/*
 * Shared (basic) types related to user configuration.
 *
 * Please try to keep dependencies on other headers minimal,
 * since config/types.h gets included in many source files.
 *
 * For the full config struct, see config/rcxml.h.
 */

/**
 * Indicates whether tablet tool motion events should be reported using
 * absolute or relative coordinates
 */
enum lab_motion {
	LAB_MOTION_ABSOLUTE = 0,
	LAB_MOTION_RELATIVE,
};

enum lab_placement_policy {
	LAB_PLACE_INVALID = 0,
	LAB_PLACE_CENTER,
	LAB_PLACE_CURSOR,
	LAB_PLACE_AUTOMATIC,
	LAB_PLACE_CASCADE,
};

enum lab_rotation {
	LAB_ROTATE_NONE = 0,
	LAB_ROTATE_90,
	LAB_ROTATE_180,
	LAB_ROTATE_270,
};

enum lab_ssd_mode {
	LAB_SSD_MODE_NONE = 0,
	LAB_SSD_MODE_BORDER,
	LAB_SSD_MODE_FULL,
	LAB_SSD_MODE_INVALID,
};

enum lab_tristate {
	LAB_STATE_UNSPECIFIED = 0,
	LAB_STATE_ENABLED,
	LAB_STATE_DISABLED
};

/*
 * This enum type is a set of bit flags where each set bit makes the
 * criteria more restrictive. For example:
 *
 * (LAB_VIEW_CRITERIA_FULLSCREEN | LAB_VIEW_CRITERIA_CURRENT_WORKSPACE)
 * matches only fullscreen views on the current workspace, while
 *
 * (LAB_VIEW_CRITERIA_ALWAYS_ON_TOP | LAB_VIEW_CRITERIA_NO_ALWAYS_ON_TOP)
 * would be contradictory and match nothing at all.
 */
enum lab_view_criteria {
	/* No filter -> all focusable views */
	LAB_VIEW_CRITERIA_NONE = 0,

	/*
	 * Includes always-on-top views, e.g.
	 * what is visible on the current workspace
	 */
	LAB_VIEW_CRITERIA_CURRENT_WORKSPACE       = 1 << 0,

	/* Positive criteria */
	LAB_VIEW_CRITERIA_FULLSCREEN              = 1 << 1,
	LAB_VIEW_CRITERIA_ALWAYS_ON_TOP           = 1 << 2,
	LAB_VIEW_CRITERIA_ROOT_TOPLEVEL           = 1 << 3,

	/* Negative criteria */
	LAB_VIEW_CRITERIA_NO_ALWAYS_ON_TOP        = 1 << 6,
	LAB_VIEW_CRITERIA_NO_SKIP_WINDOW_SWITCHER = 1 << 7,
	LAB_VIEW_CRITERIA_NO_OMNIPRESENT          = 1 << 8,
};

/*
 * Window types are based on the NET_WM constants from X11. See:
 *   https://specifications.freedesktop.org/wm-spec/1.4/ar01s05.html#id-1.6.7
 *
 * The enum constants are intended to match wlr_xwayland_net_wm_window_type.
 * Redefining the same constants here may seem redundant, but is necessary
 * to make them available even in builds with xwayland support disabled.
 */
enum lab_window_type {
	LAB_WINDOW_TYPE_INVALID = -1,
	LAB_WINDOW_TYPE_DESKTOP = 0,
	LAB_WINDOW_TYPE_DOCK,
	LAB_WINDOW_TYPE_TOOLBAR,
	LAB_WINDOW_TYPE_MENU,
	LAB_WINDOW_TYPE_UTILITY,
	LAB_WINDOW_TYPE_SPLASH,
	LAB_WINDOW_TYPE_DIALOG,
	LAB_WINDOW_TYPE_DROPDOWN_MENU,
	LAB_WINDOW_TYPE_POPUP_MENU,
	LAB_WINDOW_TYPE_TOOLTIP,
	LAB_WINDOW_TYPE_NOTIFICATION,
	LAB_WINDOW_TYPE_COMBO,
	LAB_WINDOW_TYPE_DND,
	LAB_WINDOW_TYPE_NORMAL,

	LAB_WINDOW_TYPE_LEN
};

enum window_switcher_order {
	WINDOW_SWITCHER_ORDER_FOCUS,
	WINDOW_SWITCHER_ORDER_AGE,
};

enum cycle_osd_style {
	CYCLE_OSD_STYLE_CLASSIC,
	CYCLE_OSD_STYLE_THUMBNAIL,
};

enum cycle_osd_output_criteria {
	CYCLE_OSD_OUTPUT_ALL,
	CYCLE_OSD_OUTPUT_CURSOR,
	CYCLE_OSD_OUTPUT_FOCUSED,
};

#endif /* LABWC_CONFIG_TYPES_H */
