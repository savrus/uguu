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
    int error;
};

/* allocate a new buf_str
 * returns NULL on failure */
struct buf_str *buf_alloc();

/* chop string by writing '\0' to position len if it is smaller than length */
void buf_chop(struct buf_str *bs, size_t len);

/* clear buf_str */
void buf_clear(struct buf_str *bs);

/* append a string
 * returns 0 on failure, otherwise appended string length */
size_t buf_append(struct buf_str *bs, const char *s);

/* append a string of length at most n
 * returns 0 on failure, otherwise appended string length */
size_t buf_appendn(struct buf_str *bs, const char *s, size_t n);

/* append a formatted string
 * returns 0 on failure, otherwise appended string length */
size_t buf_appendf(struct buf_str *bs, const char *fmt, ...);

/* append a formatted string
 * returns 0 on failure, otherwise appended string length */
size_t buf_vappendf(struct buf_str *bs, const char *fmt, va_list ap);

/* get a string */
const char* buf_string(struct buf_str *bs);

/* return string lenth */
size_t buf_strlen(struct buf_str *bs);

/* check if error occured while working with buffer */
char buf_error(struct buf_str *bs);

/* free a buffer */
void buf_free(struct buf_str *bs);

#ifdef __cplusplus
}
#endif

#ifdef _MSC_VER
/* C99 support */
#define va_copy(dst,src) (dst) = (src)
#endif

#endif /* BUF_H */
