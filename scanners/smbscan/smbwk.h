/* smbwk - definitions and interfaces of smb walker
 *
 * Copyright 2009, 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#ifndef SMBWK_H
#define SMBWK_H

#include <libsmbclient.h>
#include "dt.h"

/* we omit paths which length exceeds this. */
#define SMBWK_PATH_MAX_LEN 1024
/* assume most filenames don't exceed this. */
#define SMBWK_FILENAME_LEN 256

struct smbwk_dir {
    SMBCCTX *ctx;
    char *url;
    size_t url_len;
    int fd;
    int fd_real;
};

/* initialise the walker */
int smbwk_open(struct smbwk_dir *c, char *host);
/* close the walker */
int smbwk_close(struct smbwk_dir *c);

/* smb walker for 'dir tree' engine */
extern struct dt_walker smbwk_walker;

#endif /* SMBWK_H */
