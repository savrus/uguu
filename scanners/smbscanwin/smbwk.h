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

/* we omit paths which length exceeds this. */
#define SMBWK_PATH_MAX_LEN 1024
/* assume most filenames don't exceed this. */
#define SMBWK_FILENAME_LEN 256

#define SMBWK_GROW_SHARELIST 4096

struct smbwk_dir {
    wchar_t *url;
    size_t url_len;
    char *next_share;
    char *share_list;
    int subdir;
    HANDLE find;
	WIN32_FIND_DATA data;
};

typedef enum {
	ENUM_ALL = 0,
	ENUM_SKIP_ADMIN,
	ENUM_SKIP_DOLLAR
} enum_type;

/* initialise the walker */
int smbwk_open(struct smbwk_dir *c, wchar_t *host, wchar_t *username, wchar_t *password, enum_type enum_hidden_shares);
/* close the walker */
int smbwk_close(struct smbwk_dir *c);

/* smb walker for 'dir tree' engine */
extern struct dt_walker smbwk_walker;

#endif /* SMBWK_H */
