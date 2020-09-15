/*
 * We generally use Openbox defaults, but if no theme file can be found it's
 * better to populate the theme variables with some sane values as no-one
 * wants to use openbox without a theme - it'll all just be black and white.
 *
 * Openbox doesn't actual start if it can't find a theme. As it's normally
 * packaged with Clearlooks, this is not a problem, but for labwc I thought
 * this was a bit hard-line. People might want to try labwc without having
 * Openbox (and associated themes) installed.
 */

#include "theme/theme.h"

/* clang-format off */
void theme_builtin(void)
{
	parse_hexstr("#589bda", theme.window_active_title_bg_color);
	parse_hexstr("#3c7cb7", theme.window_active_handle_bg_color);
	parse_hexstr("#efece6", theme.window_inactive_title_bg_color);
	parse_hexstr("#ffffff", theme.window_active_button_unpressed_image_color);
	parse_hexstr("#000000", theme.window_inactive_button_unpressed_image_color);
}
/* clang-format on */
