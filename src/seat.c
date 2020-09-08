#include "labwc.h"

static struct wlr_seat *current_seat;

void seat_init(struct wlr_seat *seat)
{
	current_seat = seat;
}

void seat_focus_surface(struct wlr_surface *surface)
{
	if (!surface) {
		wlr_seat_keyboard_notify_clear_focus(current_seat);
		return;
	}
	/* TODO: add keyboard stuff */
	wlr_seat_keyboard_notify_enter(current_seat, surface, NULL, 0, NULL);
}

struct wlr_surface *seat_focused_surface(void)
{
	return current_seat->keyboard_state.focused_surface;
}
