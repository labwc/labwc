#ifndef __LABWC_SSD_H
#define __LABWC_SSD_H

enum ssd_part_type {
	LAB_SSD_NONE = 0,
	LAB_SSD_BUTTON_CLOSE,
	LAB_SSD_BUTTON_MAXIMIZE,
	LAB_SSD_BUTTON_ICONIFY,
	LAB_SSD_PART_TITLE,
	LAB_SSD_PART_TOP,
	LAB_SSD_PART_RIGHT,
	LAB_SSD_PART_BOTTOM,
	LAB_SSD_PART_LEFT,
	LAB_SSD_PART_CORNER_TOP_RIGHT,
	LAB_SSD_PART_CORNER_TOP_LEFT,
	LAB_SSD_END_MARKER
};

struct ssd_part {
	struct wlr_box box;
	enum ssd_part_type type;
	struct {
		struct wlr_texture *active;
		struct wlr_texture *inactive;
	} texture;
	struct {
		float *active;
		float *inactive;
	} color;
	struct wl_list link;
};

struct view;

struct border ssd_thickness(struct view *view);
struct wlr_box ssd_max_extents(struct view *view);
struct wlr_box ssd_box(struct view *view, enum ssd_part_type type);
enum ssd_part_type ssd_at(struct view *view, double lx, double ly);
void ssd_create(struct view *view);
void ssd_destroy(struct view *view);
void ssd_update_geometry(struct view *view);

#endif /* __LABWC_SSD_H */
