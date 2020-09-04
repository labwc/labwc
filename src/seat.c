#include "labwc.h"

void seat_focus_surface(struct wlr_surface *surface)
{
	if (!surface) {
		wlr_seat_keyboard_notify_clear_focus(server.seat);
		return;
	}
	/* TODO: add keyboard stuff */
	wlr_seat_keyboard_notify_enter(server.seat, surface, NULL, 0, NULL);
}
