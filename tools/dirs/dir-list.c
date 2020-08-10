#include <stdio.h>
#include <stdlib.h>

#include "common/dir.h"

int main()
{
	setenv("LABWC_DEBUG_DIR_CONFIG_AND_THEME", "1", 1);
	setenv("XDG_CONFIG_HOME", "/a:/bbb:/ccccc:/etc/foo", 1);
	printf("%s\n", config_dir());	
	printf("%s\n", theme_dir("Numix"));
}
