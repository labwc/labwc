#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "rcxml.h"

static uint32_t parse_modifier(const char *symname)
{
	if (!strcmp(symname, "S"))
		return WLR_MODIFIER_SHIFT;
	else if (!strcmp(symname, "C"))
		return WLR_MODIFIER_CTRL;
	else if (!strcmp(symname, "A"))
		return WLR_MODIFIER_ALT;
	else if (!strcmp(symname, "W"))
		return WLR_MODIFIER_LOGO;
	else
		return 0;
}

void keybind_add(struct wl_list *keybinds, const char *keybind,
		 const char *action)
{
	struct keybind *k = calloc(1, sizeof(struct keybind));
	xkb_keysym_t keysyms[32];
	gchar **symnames = g_strsplit(keybind, "-", -1);
	for (int i = 0; symnames[i]; i++) {
		char *symname = symnames[i];
		uint32_t modifier = parse_modifier(symname);
		if (modifier != 0) {
			k->modifiers |= modifier;
		} else {
			xkb_keysym_t sym = xkb_keysym_from_name(
				symname, XKB_KEYSYM_NO_FLAGS);
			if (sym == XKB_KEY_NoSymbol) {
				fprintf(stderr, "unknown keybind (%s)\n",
					symname);
				free(k);
				k = NULL;
				break;
			}
			keysyms[k->keysyms_len] = sym;
			k->keysyms_len++;
		}
	}
	g_strfreev(symnames);
	if (!k)
		return;
	wl_list_insert(keybinds, &k->link);
	k->action = strdup(action);
	k->keysyms = malloc(k->keysyms_len * sizeof(xkb_keysym_t));
	memcpy(k->keysyms, keysyms, k->keysyms_len * sizeof(xkb_keysym_t));
}

void keybind_init()
{
	keybind_add(&rc.keybinds, "A-Escape", "Exit");
	keybind_add(&rc.keybinds, "A-Tab", "NextWindow");
	keybind_add(&rc.keybinds, "A-F3", "Execute");
}

void keybind_print()
{
	struct keybind *keybind;
	wl_list_for_each_reverse (keybind, &rc.keybinds, link) {
		printf("KEY=%s-", keybind->action);
		for (size_t i = 0; i < keybind->keysyms_len; i++)
			printf("    %d\n", keybind->keysyms[i]);
	}
}
