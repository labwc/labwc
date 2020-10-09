#ifndef __LABWC_STRING_HELPERs_H
#define __LABWC_STRING_HELPERS_H

/**
 * string_strip - strip white space left and right
 * Note: this function does a left skip, so the returning pointer cannot be
 * used to free any allocated memory
 */
char *string_strip(char *s);

#endif /* __LABWC_STRING_HELPERs_H */
