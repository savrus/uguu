/* smbwk - definitions and interfaces of smb walker
 *
 * Copyright 2009, 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#ifndef SMBWK_H
#define SMBWK_H

#include <libsmbclient.h>
#include "dt.h"
#include "stack.h"
#include "buf.h"

/* specify how to deal with hidden/admin smb root directories */
enum {
    SKIP_BUCKS_NONE,
    SKIP_BUCKS_ADMIN,
    SKIP_BUCKS_ALL
};

/* walker context. */
struct smbwk_dir {
    SMBCCTX *ctx;
    struct buf_str *url;
    struct stack *ancestors;
    int fd;
    int fd_real;
    int skip_bucks;
};

/* type for element of the ancestors stack */
struct smbwk_urlpath {
    size_t urlpos;
    struct stack parent;
};

/* initialise the walker */
int smbwk_open(struct smbwk_dir *c, char *host, int skip_bucks);
/* close the walker */
int smbwk_close(struct smbwk_dir *c);

/* smb walker for 'dir tree' engine */
extern struct dt_walker smbwk_walker;

#endif /* SMBWK_H */
