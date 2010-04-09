/* umd5 - definitions and interfaces of md5 calculation
 *
 * Copyright 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#ifndef _UMD5_H_
#define _UMD5_H_

#include <stddef.h>
#ifdef _MSC_VER
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
#else
#include <inttypes.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define UMD5_BLOCK_SIZE (4 * 16)
#define UMD5_VALUE_SIZE 16

struct umd5_ctx {
    uint64_t len;
    char block[UMD5_BLOCK_SIZE];
    uint32_t A, B, C, D;
};

/* initialize context */
void umd5_init(struct umd5_ctx *ctx);

/* update */
void umd5_update(struct umd5_ctx *ctx, const char *s, size_t size);

/* finish */
void umd5_finish(struct umd5_ctx *ctx);

/* get MD5 value. buf must be of size UMD5_VALUE_SIZE */
void umd5_value(struct umd5_ctx *ctx, char *buf);

/* compare two values, returns 0 if they are same, 1 otherwise */
int umd5_cmpval(const char *s1, const char *s2);

#ifdef __cplusplus
};
#endif

#endif
