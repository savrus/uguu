/* umd5 - definitions and interfaces of md5 calculation
 *
 * Copyright 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#include <inttypes.h>
#include <stddef.h>

#define UMD5_BLOCK_SIZE (4 * 16)

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

/* get MD5 value. */
void umd5_value(struct umd5_ctx *ctx, char *buf);

