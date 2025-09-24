/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_TRANSLATE_H
#define LABWC_TRANSLATE_H
#include "config.h"

#if HAVE_NLS
#include <libintl.h>
#include <locale.h>
#define _ gettext
#else
#define _(s) (s)
#endif

#endif /* LABWC_TRANSLATE_H */
