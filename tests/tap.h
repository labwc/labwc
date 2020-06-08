/*
 * Minimalist, partial TAP implementation
 *
 * Copyright Johan Malm 2020
 */

#ifndef TAP_H
#define TAP_H

#define ok1(__x__) (ok(__x__, "%s", #__x__))

void plan(int nr_tests);
void diag(const char *fmt, ...);
int ok(int result, const char *test_name, ...);
int exit_status(void);

#endif /* TAP_H */
