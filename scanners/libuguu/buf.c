/* buf - self-enlarging buffer to store string
 *
 * Copyright 2009, 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "buf.h"
#include "log.h"

/* realloc buffer. returns 0 if failed, 1 if succeeded */
static int buf_realloc(struct buf_str *bs, size_t size)
{
    char *c;
    LOG_ASSERT(bs != NULL, "Bad arguments\n");
    
    c = (char *) realloc(bs->s, size * sizeof(char));
    if (c == NULL) {
        LOG_ERR("realloc() returned NULL\n");
        bs->error = 1;
        return 0;
    }
    bs->s = c;
    bs->size = size;
    return 1;
}

static int buf_grow(struct buf_str *bs, size_t len)
{
    LOG_ASSERT(bs != NULL, "Bad arguments\n");
    
    if (bs->length + len >= bs->size)
        return buf_realloc(bs, (bs->size + len + BUF_SIZE_STEP));
    return 1;
}

struct buf_str *buf_alloc()
{
    struct buf_str *bs;

    bs = (struct buf_str *) malloc(sizeof(struct buf_str));
    if (bs == NULL) {
        LOG_ERR("malloc() returned NULL\n");
        return NULL;
    }

    bs->s = (char *) malloc(BUF_SIZE_STEP * sizeof(char));
    if (bs->s == NULL) {
        LOG_ERR("malloc() returned NULL\n");
        free(bs);
        return NULL;
    }

    bs->s[0] = 0;
    bs->length = 0;
    bs->size = BUF_SIZE_STEP;
    bs->error = 0;
    return bs;
}

void buf_chop(struct buf_str *bs, size_t len)
{
    LOG_ASSERT(bs != NULL, "Bad arguments\n");
    
    if (len + BUF_SIZE_STEP < bs->size)
        buf_realloc(bs, len + BUF_SIZE_STEP);

    if (len < bs->length) {
        bs->s[len] = 0;
        bs->length = len;
    }
}

void buf_clear(struct buf_str *bs)
{
    LOG_ASSERT(bs != NULL, "Bad arguments\n");
    buf_chop(bs, 0);
}

size_t buf_append(struct buf_str *bs, const char *s)
{
    size_t n;
    LOG_ASSERT((bs != NULL) && (s != NULL), "Bad arguments\n");
    
    n = strlen(s);

    if (buf_grow(bs, n) == 0)
        return 0;
    strcpy(bs->s + bs->length, s);
    bs->length += n;
    return n;
}

size_t buf_appendn(struct buf_str *bs, const char *s, size_t n)
{
    size_t m;
    LOG_ASSERT((bs != NULL) && (s != NULL), "Bad arguments\n");

    m = strlen(s);
    n = (m < n) ? m : n;

    if (buf_grow(bs, n) == 0)
        return 0;
    strncpy(bs->s + bs->length, s, n);
    bs->length += n;
    bs->s[bs->length] = 0;
    return n;
}

size_t buf_vappendf(struct buf_str *bs, const char *fmt, va_list ap)
{
    int ret;
    va_list aq;
    size_t oldlen;
    LOG_ASSERT((bs != NULL) && (fmt != NULL), "Bad arguments\n");
    
    oldlen = bs->length;
    va_copy(aq, ap);
    ret = vsnprintf(bs->s + bs->length, bs->size - bs->length, fmt, aq);
    va_end(aq);
    while ((ret < 0) || ((size_t) ret >= bs->size - bs->length)) {
        size_t uret = (ret < 0) ? BUF_SIZE_STEP : (size_t) ret;
        if (buf_grow(bs, uret) == 0) {
            bs->s[oldlen] = 0;
            return 0;
        }
        va_copy(aq, ap);
        ret = vsnprintf(bs->s + bs->length, bs->size - bs->length, fmt, aq);
        va_end(aq);
    }
    bs->length += (size_t) ret;

    return ret;
}

size_t buf_appendf(struct buf_str *bs, const char *fmt, ...)
{
    size_t ret;
    va_list ap;
    LOG_ASSERT((bs != NULL) && (fmt != NULL), "Bad arguments\n");

    va_start(ap, fmt);
    ret = buf_vappendf(bs, fmt, ap);
    va_end(ap);

    return ret;
}

const char* buf_string(struct buf_str *bs)
{
    LOG_ASSERT(bs != NULL, "Bad arguments\n");
    return bs->s;
}

size_t buf_strlen(struct buf_str *bs)
{
    LOG_ASSERT(bs != NULL, "Bad arguments\n");
    return bs->length;
}

char buf_error(struct buf_str *bs)
{
    LOG_ASSERT(bs != NULL, "Bad arguments\n");
    return bs->error;
}

void buf_free(struct buf_str *bs)
{
    LOG_ASSERT(bs != NULL, "Bad arguments\n");
    free(bs->s);
    free(bs);
}

