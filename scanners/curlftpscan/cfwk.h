/* cfwk - definitions and interfaces of curl ftp walker
 *
 * Copyright 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#ifndef WDWK_H
#define WDWK_H

#include <curl/curl.h>
#include <iconv.h>

#include "dt.h"
#include "stack.h"
#include "buf.h"
#include "ftpparse.h"

/* walker context. */
struct cfwk_dir {
    CURL *curl;
    char* port;
    struct buf_str *url;
    struct stack *ancestors;
    struct stack *resources;
    struct ftpparse ftpparse;
    struct buf_str *tmpres;
    iconv_t conv_from_server;
    iconv_t conv_to_server;
    char *conv_buffer;
    size_t conv_buffer_size;
};

/* type for element of the ancestors stack */
struct cfwk_urlpath {
    size_t urlpos;
    struct stack parent;
};

struct cfwk_res {
    int type;
    char *name;
    unsigned long long size;
    struct stack stack_res;
};

/* initialise the walker */
int cfwk_open(struct cfwk_dir *c, const char *host, const char *port, const char *cp);
/* close the walker */
int cfwk_close(struct cfwk_dir *c);

/* curl ftp walker for 'dir tree' engine */
extern struct dt_walker cfwk_walker;

#endif /* WDWK_H */
