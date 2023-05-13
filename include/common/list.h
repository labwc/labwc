/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_LIST_H
#define LABWC_LIST_H
#include <wayland-server-core.h>

/**
 * wl_list_append() - add a new element to the end of a list
 * @list: list head to add it before
 * @elm: new element to be added (link of the containing struct to be precise)
 *
 * Note: In labwc, most lists are queues where we want to add new elements to
 * the end of the list. As wl_list_insert() adds elements at the front of the
 * list (like a stack) - without this helper-function - we have to use
 * wl_list_insert(list.prev, element) which is verbose and not intuitive to
 * anyone new to this API.
 */
static inline void
wl_list_append(struct wl_list *list, struct wl_list *elm)
{
	wl_list_insert(list->prev, elm);
}

#endif /* LABWC_LIST_H */
