/* dt.h - definitions and interfaces of 'dir tree' engine
 *
 * Copyright 2009, savrus
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Uguu Team nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DT_H
#define DT_H

#include <sys/types.h>

typedef enum {
    DT_FILE = 1,
    DT_DIR
} dt_type;

struct dt_dentry {
    dt_type type;
    char *name;
    off_t size;
    struct dt_dentry *parent;
    struct dt_dentry *sibling;
    struct dt_dentry *child;
    int stamp;
};

/* all must return < 0 or NULL on fail
 * dt_readdir_fn - must fill type, name and size fields of dt_dentry struct,
 * others must be null;
 *
 * Walking algorithm: dt_mktree executes dt_readdir as many times
 * as necessary to read all curdir files/subdirs. After that goup, gosibling
 * or gochild are executed to navigate fs. After each gochild and gosibling
 * readdir is executed to read all content of the directory.
 */
typedef int (*dt_init_fn) (void *curdir);
typedef int (*dt_fini_fn) (void *curdir);
typedef struct dt_dentry * (*dt_readdir_fn) (void *curdir);
typedef int (*dt_goparent_fn) (void *curdir);
typedef int (*dt_gosibling_fn) (char *name, void *curdir);
typedef int (*dt_gochild_fn) (char *name, void *curdir);

struct dt_walker {
    dt_init_fn init;
    dt_fini_fn fini;
    dt_readdir_fn readdir;
    dt_goparent_fn goparent;
    dt_gosibling_fn gosibling;
    dt_gochild_fn gochild;
};

/* root must have it's type, name and size set */
void dt_mktree(struct dt_walker *wk, struct dt_dentry *root, void *curdir);
void dt_printtree(struct dt_dentry *root);
void dt_free(struct dt_dentry *root);


#endif /* DT_H */

