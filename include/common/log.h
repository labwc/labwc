#ifndef __LABWC_LOG_H
#define __LABWC_LOG_H

/**
 * info - print info message
 */
void info(const char *msg, ...);

/**
 * warn - print warning
 */
void warn(const char *err, ...);

/**
 * die - print fatal message and exit()
 */
void die(const char *err, ...);

#endif /* __LABWC_LOG_H */
