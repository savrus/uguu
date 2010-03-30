/* buf - definitions and interfaces of self-enlarging buffer
 *
 * Copyright 2009, 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#ifndef BUF_H
#define BUF_H

#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BUF_SIZE_STEP 128

struct buf_str {
    char *s;
    size_t length;
    size_t size;
};

/* allocate a new buf_str
 * returns NULL on failure */
struct buf_str *buf_alloc();

/* clear buf_str */
void buf_clear(struct buf_str *bs);

/* append a string
 * returns 0 on failure, otherwise resulting string length */
size_t buf_append(struct buf_str *bs, const char *s);

/* append a formatted string
 * returns 0 on failure, otherwise resulting string length */
size_t buf_appendf(struct buf_str *bs, const char *fmt, ...);

/* append a formatted string
 * returns 0 on failure, otherwise resulting string length */
size_t buf_vappendf(struct buf_str *bs, const char *fmt, va_list ap);

/* get a string */
const char* buf_string(struct buf_str *bs);

/* free a buffer */
void buf_free(struct buf_str *bs);

#ifdef __cplusplus
}
#endif

#endif /* BUF_H */
