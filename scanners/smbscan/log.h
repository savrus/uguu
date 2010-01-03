/* log.h - logging macro
 *
 * Copyright 2009, 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdlib.h>

#define LOG_ERR(...) \
do { \
    fprintf(stderr, "%s: ", __func__); \
    fprintf(stderr, __VA_ARGS__); \
} while (0)

#define LOG_ASSERT(expr, ...) \
do { \
    if (!(expr)) { \
        fprintf(stderr, "Assertion failed in file %s, func %s, line %d: ", \
                __FILE__, __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        abort(); \
    } \
} while(0)

#endif /* LOG_H */

