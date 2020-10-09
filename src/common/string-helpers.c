#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void
rtrim(char **s)
{
	size_t len = strlen(*s);
	if (!len) {
		return;
	}
	char *end = *s + len - 1;
	while (end >= *s && isspace(*end)) {
		end--;
	}
	*(end + 1) = '\0';
}

char *
string_strip(char *s)
{
	rtrim(&s);
	while (isspace(*s)) {
		s++;
	}
	return s;
}
