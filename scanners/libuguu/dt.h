/* dt.h - definitions and interfaces of 'dir tree' engine
 *
 * Copyright 2009, 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#ifndef DT_H
#define DT_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* maximum number of directories that would be scanned */
#define MAX_DIRS (1 << 16)
/* maximum number of items in a directory that would be scanned */
#define MAX_ITEMS_IN_DIR (1 << 18)

typedef enum {
    DT_DIR = 1,
    DT_FILE,
} dt_type;

struct dt_dentry {
    dt_type type;
    char *name;
    unsigned long long size;
    struct dt_dentry *parent;
    struct dt_dentry *sibling;
    struct dt_dentry *child;
    struct dt_dentry *file_child;
    unsigned int id;
    unsigned int fid;
    unsigned int items;
};

typedef enum {
    DT_GO_PARENT,
    DT_GO_SIBLING,
    DT_GO_CHILD,
} dt_go;

/*
 * dt_readdir_fn - must fill type, name and size fields of dt_dentry struct,
 * others must be null;
 *
 * The following notations will be used:
 * goup = go(DT_GO_PARENT)
 * gosibling = go(DT_GO_SIBLING)
 * gochild = go(DT_GO_CHILD)
 *
 * Walking algorithm: starting from the root of the share 'dir tree' walks
 * the filesystem using goup(), gosibling() or gochild(). In each directory
 * (including the root) readdir() is executed as many times as necessary to
 * get all curdir files/subdirs.
 * After that navigatoin is performed in the following order:
 * if there is a non-visited subdirectory go there by executing gochild(),
 * if there is a non-visited sibling directory go there by executing
 * gosibling(),
 * else go up by executing goup()
 * One may have noticed that gosibling() is equivalent to goup() combined with
 * gochild() for next non-visited child.
 * If gosibling() or gochild() fails then current directory should not be
 * changed. But since readdir() will not be invoked, for gosibling() it is not
 * that strict (however, walker must ratain the same level in the file tree).
 * If goup() fails then 'dir tree' is terminated.
 * The following conditions are guaranteed:
 *   gosibling() is executed only for siblings obtained by readir()
 *   gochild() is executed only for childs obtained by realdir()
 *   goup() is never executed when we are in the root of the share
 *   readdir() is executed exactly once for each directory including the root
 *
 * curdir is the data structure that contains an abstract pointer into
 * current directory for external walker. curdir must be initialized before
 * calling dt_full() or dt_reverse()
 */

typedef struct dt_dentry * (*dt_readdir_fn) (void *curdir);
typedef int (*dt_go_fn) (dt_go type, char *name, void *curdir);

struct dt_walker {
    /* returns a single item from a direcotry.
     * readdir must return NULL on failure or when no new items are left */
    dt_readdir_fn readdir;
    /* go is used to navigate the fs 
     * go must return negative value on failure */
    dt_go_fn go;
};

/* root must have it's type, name and size set */
void dt_full(struct dt_walker *wk, struct dt_dentry *root, void *curdir);
void dt_reverse(struct dt_walker *wk, struct dt_dentry *root, void *curdir);

/* walker should use the following function to allocate dt_dentry struct */
struct dt_dentry * dt_alloc();
void dt_free(struct dt_dentry *d);

#ifdef __cplusplus
}
#endif

#endif /* DT_H */

