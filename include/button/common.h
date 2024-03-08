/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_BUTTON_COMMON_H
#define LABWC_BUTTON_COMMON_H

/**
 * button_filename() - Get full filename for button.
 * @name: The name of the button (for example 'iconify.xbm').
 * @buf: Buffer to fill with the full filename
 * @len: Length of buffer
 *
 * Example return value: /usr/share/themes/Numix/openbox-3/iconify.xbm
 */
void button_filename(const char *name, char *buf, size_t len);

#endif /* LABWC_BUTTON_COMMON_H */
