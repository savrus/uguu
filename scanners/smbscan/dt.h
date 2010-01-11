/* dt.h - definitions and interfaces of 'dir tree' engine
 *
 * Copyright 2009, 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#ifndef DT_H
#define DT_H

#include <sys/types.h>

typedef enum {
    DT_DIR = 1,
    DT_FILE,
} dt_type;

typedef enum {
    DT_OUT_SIMPLIFIED,
    DT_OUT_FULL,
    DT_OUT_REVERSE
} dt_out;

struct dt_dentry {
    dt_type type;
    char *name;
    unsigned long long size;
    struct dt_dentry *parent;
    struct dt_dentry *sibling;
    struct dt_dentry *child;
    struct dt_dentry *file_child;
    int stamp;
    unsigned int id;
    unsigned int fid;
    unsigned int total;
};

typedef enum {
    DT_GO_PARENT,
    DT_GO_SIBLING,
    DT_GO_CHILD,
} dt_go;

/* all must return < 0 or NULL on fail
 * dt_readdir_fn - must fill type, name and size fields of dt_dentry struct,
 * others must be null;
 *
 * Walking algorithm: dt_mktree executes dt_readdir as many times
 * as necessary to read all curdir files/subdirs. After that goup, gosibling
 * or gochild are executed to navigate fs. After each gochild and gosibling
 * readdir is executed to read all content of the directory.
 *
 * goup = go(DT_GO_PARENT)
 * gosibling = go(DT_GO_SIBLING)
 * gochild = go(DT_GO_CHILD)
 */
typedef int (*dt_init_fn) (void *curdir);
typedef int (*dt_fini_fn) (void *curdir);
typedef struct dt_dentry * (*dt_readdir_fn) (void *curdir);
typedef int (*dt_go_fn) (dt_go type, char *name, void *curdir);

struct dt_walker {
    dt_init_fn init;
    dt_fini_fn fini;
    dt_readdir_fn readdir;
    dt_go_fn go;
};

/* root must have it's type, name and size set */
void dt_mktree(struct dt_walker *wk, struct dt_dentry *root, void *curdir, dt_out out);
void dt_printtree(struct dt_dentry *root, dt_out out);
void dt_free(struct dt_dentry *root);
void dt_singlewalk(struct dt_walker *wk, struct dt_dentry *root, void *curdir, dt_out out);


#endif /* DT_H */

