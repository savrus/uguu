/* buf - self-enlarging buffer to store string
 *
 * Copyright 2009, 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#include "buf.h"
#include "log.h"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

struct buf_str *buf_alloc()
{
    struct buf_str *bs;

    bs = (struct buf_str *) malloc(sizeof(struct buf_str *));
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
    return bs;
}

void buf_clear(struct buf_str *bs)
{
    /* FIXME: maybe reduce bs->s to default size here? */

    bs->length = 0;
}

size_t buf_append(struct buf_str *bs, const char *s)
{
    size_t n;
    n = strlen(s);
    char *c;

    if (bs->length + n >= bs->size) {
        c = realloc(bs->s, (bs->size + n) * sizeof(char));
        if (c == NULL) {
            LOG_ERR("realloc() returned NULL\n");
            return 0;
        }
        bs->s = c;
        bs->size += n;
    }
    strcpy(bs->s + bs->length, s);
    bs->length += n;
    return bs->length;
}

size_t buf_vappendf(struct buf_str *bs, const char *fmt, va_list ap)
{
    int ret;
    char *c;
    va_list aq;

    va_copy(aq, ap);
    ret = vsnprintf(bs->s + bs->length, bs->size - bs->length, fmt, aq);
    va_end(aq);
    while (ret >= bs->size - bs->length) {
        c = (char *) realloc(bs->s, (bs->size + ret) * sizeof(char));
        if (c == NULL) {
            LOG_ERR("realloc() returned NULL\n");
            return 0;
        }
        bs->s = c;
        bs->size += ret;
        va_copy(aq, ap);
        ret = vsnprintf(bs->s + bs->length, bs->size - bs->length, fmt, aq);
        va_end(aq);
    }
    bs->length += ret;

    return bs->length;
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

void buf_free(struct buf_str *bs)
{
    free(bs->s);
    free(bs);
}

