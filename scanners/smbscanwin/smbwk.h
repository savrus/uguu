/* smbwk - definitions and interfaces of smb walker
 *
 * Copyright 2009, 2010, savrus
 * Copyright 2010, Radist <radist.nt@gmail.com>
 * Read the COPYING file in the root of the source tree.
 */

#ifndef SMBWK_H
#define SMBWK_H

#include <Windows.h>
#include "dt.h"
#include "stack.h"
#include "wbuf.h"

#define SMBWK_GROW_SHARELIST 4096

/* walker context. */
struct smbwk_dir {
    struct wbuf_str *url;
    struct stack *ancestors;
    char *next_share;
    char *share_list;
    int subdir;
    HANDLE find;
    WIN32_FIND_DATA data;
};

/* type for element of the ancestors stack */
struct smbwk_urlpath {
    size_t urlpos;
    struct stack parent;
};

/* specify what enum type to use and how to deal with hidden/admin shares */
typedef enum {
	ENUM_ALL = 0,
	ENUM_SKIP_ADMIN,
	ENUM_SKIP_DOLLAR
} enum_type;

/* initialise the walker */
int smbwk_open(struct smbwk_dir *c, wchar_t *host, wchar_t *username, wchar_t *password, enum_type enum_hidden_shares, int *wnet_cancel);
/* close the walker */
int smbwk_close(struct smbwk_dir *c, int wnet_cancel);

/* smb walker for 'dir tree' engine */
extern struct dt_walker smbwk_walker;

#endif /* SMBWK_H */
