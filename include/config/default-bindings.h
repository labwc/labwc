/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_DEFAULT_BINDINGS_H
#define LABWC_DEFAULT_BINDINGS_H

static struct key_combos {
	const char *binding, *action;
	struct {
		const char *name, *value;
	} attributes[2];
} key_combos[] = { {
		.binding = "A-Tab",
		.action = "NextWindow",
	}, {
		.binding = "W-Return",
		.action = "Execute",
		.attributes[0] = {
			.name = "command",
			.value = "alacritty",
		},
	}, {
		.binding = "A-F3",
		.action = "Execute",
		.attributes[0] = {
			.name = "command",
			.value = "bemenu-run",
		},
	}, {
		.binding = "A-F4",
		.action = "Close",
	}, {
		.binding = "W-a",
		.action = "ToggleMaximize",
	}, {
		.binding = "A-Left",
		.action = "MoveToEdge",
		.attributes[0] = {
			.name = "direction",
			.value = "left",
		},
	}, {
		.binding = "A-Right",
		.action = "MoveToEdge",
		.attributes[0] = {
			.name = "direction",
			.value = "right",
		},
	}, {
		.binding = "A-Up",
		.action = "MoveToEdge",
		.attributes[0] = {
			.name = "direction",
			.value = "up",
		},
	}, {
		.binding = "A-Down",
		.action = "MoveToEdge",
		.attributes[0] = {
			.name = "direction",
			.value = "down",
		},
	}, {
		.binding = "W-Left",
		.action = "SnapToEdge",
		.attributes[0] = {
			.name = "direction",
			.value = "left",
		},
	}, {
		.binding = "W-Right",
		.action = "SnapToEdge",
		.attributes[0] = {
			.name = "direction",
			.value = "right",
		},
	}, {
		.binding = "W-Up",
		.action = "SnapToEdge",
		.attributes[0] = {
			.name = "direction",
			.value = "up",
		},
	}, {
		.binding = "W-Down",
		.action = "SnapToEdge",
		.attributes[0] = {
			.name = "direction",
			.value = "down",
		},
	}, {
		.binding = "A-Space",
		.action = "ShowMenu",
		.attributes[0] = {
			.name = "menu",
			.value = "client-menu"
		},
		.attributes[1] = {
			.name = "atCursor",
			.value = "no",
		},
	}, {
		.binding = "XF86_AudioLowerVolume",
		.action = "Execute",
		.attributes[0] = {
			.name = "command",
			.value = "amixer sset Master 5%-",
		},
	}, {
		.binding = "XF86_AudioRaiseVolume",
		.action = "Execute",
		.attributes[0] = {
			.name = "command",
			.value = "amixer sset Master 5%+",
		},
	}, {
		.binding = "XF86_AudioMute",
		.action = "Execute",
		.attributes[0] = {
			.name = "command",
			.value = "amixer sset Master toggle",
		},
	}, {
		.binding = "XF86_MonBrightnessUp",
		.action = "Execute",
		.attributes[0] = {
			.name = "command",
			.value = "brightnessctl set +10%",
		},
	}, {
		.binding = "XF86_MonBrightnessDown",
		.action = "Execute",
		.attributes[0] = {
			.name = "command",
			.value = "brightnessctl set 10%-",
		},
	}, {
		.binding = NULL,
	},
};

/*
 * `struct mouse_combo` variable description and examples:
 *
 * | Variable   | Description                | Examples
 * |------------|----------------------------|----------------------------
 * | context    | context name               | Maximize, Root
 * | button     | mousebind button/direction | Left, Up
 * | event      | mousebind action           | Click, Scroll
 * | action     | action name                | ToggleMaximize, GoToDesktop
 * |============|============================|============================
 * | Attributes |                            |
 * |------------|----------------------------|----------------------------
 * | name       | action attribute name      | to
 * | value      | action attribute value     | left
 *
 * <mouse>
 *   <context name="Maximize">
 *     <mousebind button="Left" action="Click">
 *       <action name="Focus"/>
 *       <action name="Raise"/>
 *       <action name="ToggleMaximize"/>
 *     </mousebind>
 *   </context>
 *   <context name="Root">
 *     <mousebind direction="Up" action="Scroll">
 *       <action name="GoToDesktop" to="left" wrap="yes"/>
 *     </mousebind>
 *   </context>
 * </mouse>
 */
static struct mouse_combos {
	const char *context, *button, *event, *action;
	struct {
		const char *name, *value;
	} attributes[2];
} mouse_combos[] = { {
		.context = "Left",
		.button = "Left",
		.event = "Drag",
		.action = "Resize",
	}, {
		.context = "Top",
		.button = "Left",
		.event = "Drag",
		.action = "Resize",
	}, {
		.context = "Bottom",
		.button = "Left",
		.event = "Drag",
		.action = "Resize",
	}, {
		.context = "Right",
		.button = "Left",
		.event = "Drag",
		.action = "Resize",
	}, {
		.context = "TLCorner",
		.button = "Left",
		.event = "Drag",
		.action = "Resize",
	}, {
		.context = "TRCorner",
		.button = "Left",
		.event = "Drag",
		.action = "Resize",
	}, {
		.context = "BRCorner",
		.button = "Left",
		.event = "Drag",
		.action = "Resize",
	}, {
		.context = "BLCorner",
		.button = "Left",
		.event = "Drag",
		.action = "Resize",
	}, {
		.context = "Frame",
		.button = "A-Left",
		.event = "Press",
		.action = "Focus",
	}, {
		.context = "Frame",
		.button = "A-Left",
		.event = "Press",
		.action = "Raise",
	}, {
		.context = "Frame",
		.button = "A-Left",
		.event = "Drag",
		.action = "Move",
	}, {
		.context = "Frame",
		.button = "A-Right",
		.event = "Press",
		.action = "Focus",
	}, {
		.context = "Frame",
		.button = "A-Right",
		.event = "Press",
		.action = "Raise",
	}, {
		.context = "Frame",
		.button = "A-Right",
		.event = "Drag",
		.action = "Resize",
	}, {
		.context = "Titlebar",
		.button = "Left",
		.event = "Press",
		.action = "Focus",
	}, {
		.context = "Titlebar",
		.button = "Left",
		.event = "Press",
		.action = "Raise",
	}, {
		.context = "Titlebar",
		.button = "Up",
		.event = "Scroll",
		.action = "Unfocus",
	}, {
		.context = "Titlebar",
		.button = "Up",
		.event = "Scroll",
		.action = "Shade",
	}, {
		.context = "Titlebar",
		.button = "Down",
		.event = "Scroll",
		.action = "Unshade",
	}, {
		.context = "Titlebar",
		.button = "Down",
		.event = "Scroll",
		.action = "Focus",
	}, {
		.context = "Title",
		.button = "Left",
		.event = "Drag",
		.action = "Move",
	}, {
		.context = "Title",
		.button = "Left",
		.event = "DoubleClick",
		.action = "ToggleMaximize",
	}, {
		.context = "TitleBar",
		.button = "Right",
		.event = "Click",
		.action = "Focus",
	}, {
		.context = "TitleBar",
		.button = "Right",
		.event = "Click",
		.action = "Raise",
	}, {
		.context = "Title",
		.button = "Right",
		.event = "Click",
		.action = "ShowMenu",
		.attributes[0] = {
			.name = "menu",
			.value = "client-menu",
		},
		.attributes[1] = {
			.name = "atCursor",
			.value = "yes",
		},
	}, {
		.context = "Close",
		.button = "Left",
		.event = "Click",
		.action = "Close",
	}, {
		.context = "Iconify",
		.button = "Left",
		.event = "Click",
		.action = "Iconify",
	}, {
		.context = "Maximize",
		.button = "Left",
		.event = "Click",
		.action = "ToggleMaximize",
	}, {
		.context = "Maximize",
		.button = "Right",
		.event = "Click",
		.action = "ToggleMaximize",
		.attributes[0] = {
			.name = "direction",
			.value = "horizontal",
		},
	}, {
		.context = "Maximize",
		.button = "Middle",
		.event = "Click",
		.action = "ToggleMaximize",
		.attributes[0] = {
			.name = "direction",
			.value = "vertical",
		},
	}, {
		.context = "WindowMenu",
		.button = "Left",
		.event = "Click",
		.action = "ShowMenu",
		.attributes[0] = {
			.name = "menu",
			.value = "client-menu",
		},
		.attributes[1] = {
			.name = "atCursor",
			.value = "no",
		},
	}, {
		.context = "WindowMenu",
		.button = "Right",
		.event = "Click",
		.action = "ShowMenu",
		.attributes[0] = {
			.name = "menu",
			.value = "client-menu",
		},
		.attributes[1] = {
			.name = "atCursor",
			.value = "no",
		},
	}, {
		.context = "Root",
		.button = "Left",
		.event = "Press",
		.action = "ShowMenu",
		.attributes[0] = {
			.name = "menu",
			.value = "root-menu",
		},
	}, {
		.context = "Root",
		.button = "Right",
		.event = "Press",
		.action = "ShowMenu",
		.attributes[0] = {
			.name = "menu",
			.value = "root-menu",
		},
	}, {
		.context = "Root",
		.button = "Middle",
		.event = "Press",
		.action = "ShowMenu",
		.attributes[0] = {
			.name = "menu",
			.value = "root-menu",
		},
	}, {
		.context = "Root",
		.button = "Up",
		.event = "Scroll",
		.action = "GoToDesktop",
		.attributes[0] = {
			.name = "to",
			.value = "left",
		},
	}, {
		.context = "Root",
		.button = "Down",
		.event = "Scroll",
		.action = "GoToDesktop",
		.attributes[0] = {
			.name = "to",
			.value = "right",
		},
	}, {
		.context = "Client",
		.button = "Left",
		.event = "Press",
		.action = "Focus",
	}, {
		.context = "Client",
		.button = "Left",
		.event = "Press",
		.action = "Raise",
	}, {
		.context = "Client",
		.button = "Right",
		.event = "Press",
		.action = "Focus",
	}, {
		.context = "Client",
		.button = "Right",
		.event = "Press",
		.action = "Raise",
	}, {
		.context = "Client",
		.button = "Middle",
		.event = "Press",
		.action = "Focus",
	}, {
		.context = "Client",
		.button = "Middle",
		.event = "Press",
		.action = "Raise",
	}, {
		.context = NULL,
	},
};

#endif /* LABWC_DEFAULT_BINDINGS_H */
