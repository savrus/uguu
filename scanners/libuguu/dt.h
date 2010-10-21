/* dt.h - definitions and interfaces of 'dir tree' engine
 *
 * Copyright 2009, 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#ifndef DT_H
#define DT_H

#include <stdio.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* maximum number of directories that would be scanned */
#define DT_MAX_DIRS (1 << 20)
/* maximum number of items in a directory that would be scanned */
#define DT_MAX_ITEMS_IN_DIR (1 << 18)
/* number of matches to assume recursion */
#define DT_RECURSION_THRESHOLD 5


typedef enum {
    DT_DIR = 1,
    DT_FILE,
} dt_type;

#define DT_TYPE_MASK    0xff
#define DT_TYPE_NEW     0x100
#define DT_TYPE_ONCE    0x200

struct dt_dentry {
    char *name;
    char *hash;
    struct dt_dentry *parent;
    struct dt_dentry *sibling;
    struct dt_dentry *child;
    struct dt_dentry *file_child;
    unsigned long long size;
    unsigned int id;
    unsigned int fid;
    unsigned int items;
    dt_type type;
};

typedef enum {
    DT_GO_PARENT,
    DT_GO_CHILD,
} dt_go;

/*
 * dt_readdir_fn - must fill type, name and size fields of dt_dentry struct,
 * others must be null;
 *
 * The following notations will be used:
 * goup = go(DT_GO_PARENT)
 * gochild = go(DT_GO_CHILD)
 *
 * Walking algorithm: starting from the root of the share 'dir tree' walks
 * the filesystem using goup() and gochild(). In each directory
 * (including the root) readdir() is executed as many times as necessary to
 * get all curdir files/subdirs.
 * After that navigatoin is performed in the following order:
 * if there is a non-visited subdirectory go there by executing gochild(),
 * else go up by executing goup()
 * If gochild() fails then current directory should not be changed.
 * If goup() fails then 'dir tree' is terminated.
 * The following conditions are guaranteed:
 *   gochild() is executed only for childs obtained by realdir()
 *   goup() is never executed when we are in the root of the share
 *   readdir() is executed exactly once for each directory including the root
 *
 * curdir is the data structure which contains an abstract pointer into
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

struct dt_wctx {
    struct dt_dentry *d;
    struct dt_dentry *od;
    struct dt_walker *wk;
    void *curdir;
    void (*on_enter_root) (struct dt_wctx *wc);
    void (*on_enter) (struct dt_wctx *wc);
    void (*on_leave_root) (struct dt_wctx *wc);
    void (*on_leave) (struct dt_wctx *wc);
    /* return 1 if went to child, 0 if remained in the same dir */
    int (*go_child) (struct dt_wctx *wc);
    /* return 1 if went to sibling, 0 if to parent */
    int (*go_sibling_or_parent) (struct dt_wctx *wc);
    void (*call_dir) (struct dt_wctx *wc, struct dt_dentry *d);
    void (*call_file) (struct dt_wctx *wc, struct dt_dentry *d);
    char *prefix;
    unsigned int id;
};

/* root must have it's type, name and size set */
void dt_full(struct dt_walker *wk, void *curdir);
void dt_reverse(struct dt_walker *wk, void *curdir);
void dt_diff(FILE *file, struct dt_walker *wk, void *curdir);

/* walker should use the following function to allocate dt_dentry struct */
struct dt_dentry * dt_alloc();
void dt_free(struct dt_dentry *d);
void dt_rfree(struct dt_dentry *root);

#ifdef __cplusplus
}
#endif

#endif /* DT_H */

