#ifndef __LABWC_FONT_H
#define __LABWC_FONT_H

/**
 * font_height - get font vertical extents
 * @font_description: string describing font, for example 'sans 10'
 */
int font_height(const char *font_description);

/**
 * font_finish - free some font related resources
 * Note: use on exit
 */
void font_finish(void);

#endif /* __LABWC_FONT_H */
