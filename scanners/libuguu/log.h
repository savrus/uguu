/* log.h - logging macro
 *
 * Copyright 2009, 2010, savrus
 * Copyright 2010, Radist <radist.nt@gmail.com>
 * Read the COPYING file in the root of the source tree.
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#include <crtdbg.h>
#define __func__ __FUNCTION__
#endif

#include "estat.h"

#define LOG_ERR_(perr_call, ...) \
do { \
    fprintf(stderr, "%s: ", __func__); \
    fprintf(stderr, __VA_ARGS__); \
    perr_call; \
} while (0)
#ifdef WIN32
#define LOG_ERR(...) LOG_ERR_(,__VA_ARGS__)
#define LOG_ERRNO LOG_ERR
#else
#define LOG_ERR(...) LOG_ERR_(, __VA_ARGS__)
#define LOG_ERRNO(...) LOG_ERR_(perror(" errno"), __VA_ARGS__)
#endif

#define LOG_ASSERT_(expr, exit_call, ...) \
do { \
    if (!(expr)) { \
        fprintf(stderr, "Assertion failed in file %s, func %s, line %d: ", \
                __FILE__, __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        exit_call; \
    } \
} while(0)
#if defined(_MSC_VER) && defined(_DEBUG)
#define LOG_ASSERT(expr, ...) LOG_ASSERT_(expr, _CrtDbgBreak(), __VA_ARGS__)
#else
#define LOG_ASSERT(expr, ...) LOG_ASSERT_(expr, exit(ESTAT_FAILURE), __VA_ARGS__)
#endif

#ifdef __cplusplus

#define LOG_THROW(ex, ...) \
do { \
    ex _exception; \
    fprintf(stderr, "%s: ", __func__); \
    fprintf(stderr, __VA_ARGS__); \
    throw _exception; \
} while(0)

#endif

#endif /* LOG_H */

