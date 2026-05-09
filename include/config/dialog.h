/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LAB_DIALOG_H
#define LAB_DIALOG_H

struct wlr_scene_node;

/**
 * dialog_create_callback - creates the config dialog,
 * used with wl_event_loop_add_idle.
 *
 * @data: Ignored.
 */
void dialog_create_callback(void *data);

/**
 * dialog_destroy - destroys a dialog that is a child of
 * server.dialog_tree, destroys all children if NULL is provided.
 *
 * @dialog: Dialog to be destroyed
 */
void dialog_destroy(struct wlr_scene_node *dialog);

#endif /* LAB_DIALOG_H */
