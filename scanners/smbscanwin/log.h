/* log.h - logging macro
 *
 * Copyright 2009, 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdlib.h>

#include "estat.h"

#define LOG_ERR(...) \
do { \
    fprintf(stderr, "%s: ", __FUNCTION__); \
    fprintf(stderr, __VA_ARGS__); \
    /*perror(" errorno");*/ \
} while (0)

#define LOG_ASSERT(expr, ...) \
do { \
    if (!(expr)) { \
        fprintf(stderr, "Assertion failed in file %s, func %s, line %d: ", \
                __FILE__, __FUNCTION__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        exit(ESTAT_FAILURE); \
    } \
} while(0)

#endif /* LOG_H */

