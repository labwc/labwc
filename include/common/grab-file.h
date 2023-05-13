/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Read file into memory
 *
 * Copyright Johan Malm 2020
 */

#ifndef LABWC_GRAB_FILE_H
#define LABWC_GRAB_FILE_H

/**
 * grab_file - read file into memory buffer
 * @filename: file to read
 * Returns pointer to buffer. Free with free().
 */
char *grab_file(const char *filename);

#endif /* LABWC_GRAB_FILE_H */
