/* log.h - logging macro
 *
 * Copyright 2009, 2010, savrus
 * Copyright 2010, Radist <radist.nt@gmail.com>
 * Read the COPYING file in the root of the source tree.
 */

#ifndef LOGP_H
#define LOGP_H

#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#include <crtdbg.h>
#define __func__ __FUNCTION__
#endif

#define LOG_ERR(...) \
do { \
    fprintf(stderr, "%s: ", __func__); \
    fprintf(stderr, __VA_ARGS__); \
} while (0)

#ifdef _DEBUG
#define LOG_ASSERT_(expr, exit_proc, ...) \
do { \
    if (!(expr)) { \
        fprintf(stderr, "Assertion failed in file %s, func %s, line %d: ", \
                __FILE__, __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        exit_proc(); \
    } \
} while(0)
#ifdef _MSC_VER
#define LOG_ASSERT(expr, ...) LOG_ASSERT_(expr, _CrtDbgBreak, __VA_ARGS__)
#else
#define LOG_ASSERT(expr, ...) LOG_ASSERT_(expr, abort, __VA_ARGS__)
#endif
#else
#define LOG_ASSERT(...)
#endif // _DEBUG

#ifdef __cplusplus

#define LOG_THROW(ex, ...) \
do { \
	ex _exception; \
    fprintf(stderr, "%s: ", __func__); \
    fprintf(stderr, __VA_ARGS__); \
	throw _exception; \
} while(0)

#endif //__cplusplus

#endif /* LOGP_H */

