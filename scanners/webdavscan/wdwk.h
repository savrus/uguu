/* wdwk - definitions and interfaces of webdav walker
 *
 * Copyright 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#ifndef WDWK_H
#define WDWK_H

#include <neon/ne_session.h>

#include "dt.h"
#include "stack.h"
#include "buf.h"

/* walker context. */
struct wdwk_dir {
    ne_session *sess;
    char *host;
    int port;
    struct buf_str *url;
    struct stack *ancestors;
    struct stack *resources;
};

/* type for element of the ancestors stack */
struct wdwk_urlpath {
    size_t urlpos;
    struct stack parent;
};

struct wdwk_res {
    int type;
    char *name;
    unsigned long long size;
    struct stack stack_res;
};

/* initialise the walker */
int wdwk_open(struct wdwk_dir *c, char *host, int port);
/* close the walker */
int wdwk_close(struct wdwk_dir *c);

/* webdav walker for 'dir tree' engine */
extern struct dt_walker wdwk_walker;

#endif /* WDWK_H */
