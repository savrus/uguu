/* dt.h - 'dir tree' engine
 * Walks directory subtree and counts size like du(1).
 * File system navigation functions as well as readdir are
 * provided externally through dt_walker structure
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

#include "dt.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static void dt_readdir(struct dt_walker *wk, struct dt_dentry *d, void *curdir)
{
    struct dt_dentry *dp, *dn;
    
    if ((dn = wk->readdir(curdir)) != NULL) {
        dn->parent = d;
        d->child = dn;
        dn->stamp = 0;
    }
    else
        return;

    dp = dn;
    while ((dn = wk->readdir(curdir)) != NULL) {
        dp->sibling = dn;
        dn->parent = d;
        dn->stamp = 0;
        dp = dn;
    }
    dp->sibling = NULL;
}

static struct dt_dentry * dt_find_dir_sibling(struct dt_dentry *d)
{
    struct dt_dentry *dn;
    //if (d == NULL)
    //    return NULL;
    for (dn = d->sibling;
         (dn != NULL) && (dn->type != DT_DIR);
         dn = dn->sibling)
        ;
    return dn;
}

static struct dt_dentry * dt_find_dir_child(struct dt_dentry *d)
{
    struct dt_dentry *dn;
    //if (d == NULL)
    //    return NULL;
    dn = d->child;
    if ((dn != NULL) && (dn->type != DT_DIR))
        return dt_find_dir_sibling(dn);
    return dn;
}

static struct dt_dentry * dt_go_sibling_or_parent(struct dt_walker *wk, struct dt_dentry *d, void *curdir)
{
    struct dt_dentry *dn = d;
    while (((dn = dt_find_dir_sibling(dn)) != NULL)
           && (wk->gosibling(dn->name, curdir) < 0))
           ;
    if (dn == NULL) {
        dn = d->parent;
        if (wk->goparent(curdir) < 0)
            return NULL;
    }
    return dn;
}

static struct dt_dentry * dt_go_child(struct dt_walker *wk, struct dt_dentry *d, void *curdir)
{
    struct dt_dentry *dc;
    if ((dc = dt_find_dir_child(d)) == NULL)
        return NULL;
    while ((wk->gochild(dc->name, curdir) < 0)
           && ((dc = dt_find_dir_sibling(dc)) != NULL))
        ;
    return dc;
}

static struct dt_dentry * dt_next_sibling_or_parent(struct dt_dentry *d)
{
    struct dt_dentry *dc;
    if ((dc = dt_find_dir_sibling(d)) != NULL)
        return dc;
    else
        return d->parent;
}

void dt_mktree(struct dt_walker *wk, struct dt_dentry *root, void *curdir)
{
    struct dt_dentry *d, *dc;

    if ((root == NULL) || (wk == NULL)) {
        fprintf(stderr, "%s: bad arguments\n", __func__);
        return;
    }

    root->parent = NULL;
    root->sibling = NULL;
    root->child = NULL;
    root->stamp = 0;

    if (wk->init(curdir) < 0)
        return;

    dt_readdir(wk, root, curdir);

    d = dt_find_dir_child(root);

    if ((d != NULL) && (wk->gochild(d->name, curdir) > 0)) {
        // invariants:
        // d->type == DT_DIR
        // if v->stamp = 1 then descendants of v are processed
        while((d != NULL) && (d != root)) {
            if (d->stamp == 0) {
                dt_readdir(wk, d, curdir);
                if ((dc = dt_go_child(wk, d, curdir)) != NULL) {
                    d->stamp = 1;
                    d = dc;
                    continue;
                }
            } else {
                d->stamp = 0;
            }
            d = dt_go_sibling_or_parent(wk, d, curdir);
        }

        wk->fini(curdir);

        d = dt_find_dir_child(root);
        
        // invariants:
        // d->type == DT_DIR
        // if v->stamp = 1 then descendants of v are processed
        while (d != root) {
            if (d->stamp == 0) {
                if ((dc = dt_find_dir_child(d)) != NULL) {
                    d->stamp = 1;
                    d = dc;
                    continue;
                }
            } else {
                d->stamp = 0;
            }
            for (dc = d->child; dc != NULL; dc = dc->sibling)
                d->size += dc->size;
            d = dt_next_sibling_or_parent(d);
        }
    }
    for (dc = root->child; dc != NULL; dc = dc->sibling)
        root->size += dc->size;
}

static void dt_printpath(struct dt_dentry *d)
{
    //if (d == NULL)
    //    return;
    if (d->parent != NULL) {
        dt_printpath(d->parent);
        printf("/");
    }
    printf("%s",d->name);
}

static void dt_printdir(struct dt_dentry *d, char *prefix)
{
    struct dt_dentry *dc;
    //if (d == NULL)
    //    return;
    printf("%s", prefix);
    for (dc = d->child; dc != NULL; dc = dc->sibling) {
        dt_printpath(dc);
        printf("%s %zu\n", (dc->type == DT_DIR) ? "/" : "", dc->size);
    }
}
    

void dt_printtree(struct dt_dentry *root)
{
    struct dt_dentry *d, *dc;

    if (root == NULL) {
        fprintf(stderr, "%s: bad arguments\n", __func__);
        return;
    }

    dt_printdir(root, "");
    
    d = dt_find_dir_child(root);
    if (d == NULL)
        return;

    while (d != root) {
        if (d->stamp == 0) {
            dt_printdir(d, "");
            if ((dc = dt_find_dir_child(d)) != NULL) {
                d->stamp = 1;
                d = dc;
                continue;
            }
        } else { 
            d->stamp = 0;
        }
        d = dt_next_sibling_or_parent(d);
    }
}

static void dt_free_childs(struct dt_dentry *d)
{
    struct dt_dentry *dc, *dn;
    dc = d->child;
    while (dc != NULL) {
        dn = dc->sibling;
        free(dc->name);
        free(dc);
        dc = dn;
    }
}

void dt_free(struct dt_dentry *root)
{
    struct dt_dentry *d, *dc;

    if (root == NULL) {
        fprintf(stderr, "%s: bad arguments\n", __func__);
        return;
    }
    
    d = dt_find_dir_child(root);
    if (d != NULL) {
        while (d != root) {
            if (d->stamp == 0) {
                if ((dc = dt_find_dir_child(d)) != NULL) {
                    d->stamp = 1;
                    d = dc;
                    continue;
                }
            } else { 
                d->stamp = 0;
            }
            dt_free_childs(d);
            d = dt_next_sibling_or_parent(d);
        }
    }
    dt_free_childs(root);
}

