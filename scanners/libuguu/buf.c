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

void buf_clear(struct buf_str *bs)
{
    /* FIXME: maybe reduce bs->s to default size here? */

    bs->length = 0;
}

size_t buf_append(struct buf_str *bs, const char *s)
{
    size_t n = strlen(s);
    char *c;

    if (bs->length + n >= bs->size) {
        c = realloc(bs->s, (bs->size + n) * sizeof(char));
        if (c == NULL) {
            LOG_ERR("realloc() returned NULL\n");
            bs->error = 1;
            return 0;
        }
        bs->s = c;
        bs->size += n;
    }
    strcpy(bs->s + bs->length, s);
    bs->length += n;
    return n;
}

size_t buf_appendn(struct buf_str *bs, const char *s, size_t n)
{
    size_t m;
    char *c;

    m = strlen(s);
    n = (m < n) ? m : n;

    if (bs->length + n >= bs->size) {
        c = realloc(bs->s, (bs->size + n) * sizeof(char));
        if (c == NULL) {
            LOG_ERR("realloc() returned NULL\n");
            bs->error = 1;
            return 0;
        }
        bs->s = c;
        bs->size += n;
    }
    strncpy(bs->s + bs->length, s, n);
    bs->length += n;
    bs->s[bs->length] = 0;
    return n;
}

size_t buf_vappendf(struct buf_str *bs, const char *fmt, va_list ap)
{
    int ret;
    char *c;
    va_list aq;

    va_copy(aq, ap);
    ret = vsnprintf(bs->s + bs->length, bs->size - bs->length, fmt, aq);
    va_end(aq);
    while ((ret < 0) || ((size_t) ret >= bs->size - bs->length)) {
        size_t uret = (ret < 0) ? BUF_SIZE_STEP : (size_t) ret;
        c = (char *) realloc(bs->s, (bs->size + uret) * sizeof(char));
        if (c == NULL) {
            LOG_ERR("realloc() returned NULL\n");
            bs->error = 1;
            return 0;
        }
        bs->s = c;
        bs->size += uret;
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

    va_start(ap, fmt);
    ret = buf_vappendf(bs, fmt, ap);
    va_end(ap);

    return ret;
}

const char* buf_string(struct buf_str *bs)
{
    return bs->s;
}

char buf_error(struct buf_str *bs)
{
    return bs->error;
}

void buf_free(struct buf_str *bs)
{
    free(bs->s);
    free(bs);
}

