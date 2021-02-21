#include <stdlib.h>
#include "common/zfree.h"

void __zfree(void **ptr)
{
	if (!ptr || !*ptr) {
		return;
	}
	free(*ptr);
	*ptr = NULL;
}
