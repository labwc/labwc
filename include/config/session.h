#ifndef __LABWC_SESSION_H
#define __LABWC_SESSION_H

/**
 * session_environment_init - set enrivonment variables
 * Note: Same as `. ~/.config/labwc/environment` (or equivalent XDG config dir)
 */
session_environment_init(void);

/**
 * session_autostart_init - run autostart file as shell script
 * Note: Same as `sh ~/.config/labwc/autostart` (or equivalent XDG config dir)
 */
session_autostart_init(void);

#endif /* __LABWC_SESSION_H */
