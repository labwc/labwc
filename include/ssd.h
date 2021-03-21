#ifndef __LABWC_SSD_H
#define __LABWC_SSD_H

enum ssd_part {
	LAB_SSD_NONE = 0,
	LAB_SSD_BUTTON_CLOSE,
	LAB_SSD_BUTTON_MAXIMIZE,
	LAB_SSD_BUTTON_ICONIFY,
	LAB_SSD_PART_TITLE,
	LAB_SSD_PART_TOP,
	LAB_SSD_PART_RIGHT,
	LAB_SSD_PART_BOTTOM,
	LAB_SSD_PART_LEFT,
	LAB_SSD_END_MARKER
};

struct view;

struct border ssd_thickness(struct view *view);
struct wlr_box ssd_max_extents(struct view *view);
struct wlr_box ssd_box(struct view *view, enum ssd_part ssd_part);
enum ssd_part ssd_at(struct view *view, double lx, double ly);

#endif /* __LABWC_SSD_H */
