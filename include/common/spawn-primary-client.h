/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SPAWN_PRIMARY_CLIENT_H
#define LABWC_SPAWN_PRIMARY_CLIENT_H
/*
 * Copied from https://github.com/cage-kiosk/
 *
 * Copyright (c) 2018-2020 Jente Hidskes
 * Copyright (c) 2019 The Sway authors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <wayland-server-core.h>

struct server;

bool spawn_primary_client(struct server *server, const char *command,
	pid_t *pid_out, struct wl_event_source **sigchld_source);

#endif /* LABWC_SPAWN_PRIMARY_CLIENT_H */
