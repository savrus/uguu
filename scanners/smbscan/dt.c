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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "dt.h"
#include "log.h"

static void dt_printfile(struct dt_dentry *d, dt_out out);


/* link next read child into dir tree according to selectd out policy
 * d - current directory
 * dp - last linked child (NULL if none)
 * dn - newly read child
 *
 * return: dn, if dn linked, dc otherwise
 */
static struct dt_dentry * dt_linkchild(struct dt_dentry *d, struct dt_dentry *dp, struct dt_dentry *dn, dt_out out)
{
    LOG_ASSERT((d != NULL) && (dn != NULL), "Bad arguments\n");
     dn->parent = d;
     switch(out) {
        case DT_OUT_FULL:
        case DT_OUT_SIMPLIFIED:
        default:
            if (dp == NULL)
                d->child = dn;
            else
                dp->sibling = dn;
            return dn;
            break;
        case DT_OUT_REVERSE:
            if (dn->type == DT_DIR) {
                if (dp == NULL)
                    d->child = dn;
                else
                    dp->sibling = dn;
                return dn;
            } else {
                dt_printfile(dn, out);
                d->size += dn->size;
                free(dn->name);
                free(dn);
                return dp;
            }
    }
}

static void dt_readdir(struct dt_walker *wk, struct dt_dentry *d, void *curdir, unsigned int *id, dt_out out)
{
    struct dt_dentry *dp = NULL, *dn;
    LOG_ASSERT((wk != NULL) && (d != NULL), "Bad arguments\n");
    
    while ((dn = wk->readdir(curdir)) != NULL) {
        if (dn->type == DT_DIR)
            dn->id = (*id)++;
        dn->stamp = 0;
        dp = dt_linkchild(d, dp, dn, out);
    }
    if (dp != NULL)
        dp->sibling = NULL;
}

static struct dt_dentry * dt_find_dir_sibling(struct dt_dentry *d)
{
    struct dt_dentry *dn;
    LOG_ASSERT(d != NULL, "Bad arguments\n");
    for (dn = d->sibling;
         (dn != NULL) && (dn->type != DT_DIR);
         dn = dn->sibling)
        ;
    return dn;
}

static struct dt_dentry * dt_find_dir_child(struct dt_dentry *d)
{
    struct dt_dentry *dn;
    LOG_ASSERT(d != NULL, "Bad arguments\n");
    dn = d->child;
    if ((dn != NULL) && (dn->type != DT_DIR))
        return dt_find_dir_sibling(dn);
    return dn;
}

static struct dt_dentry * dt_go_sibling_or_parent(struct dt_walker *wk, struct dt_dentry *d, void *curdir)
{
    struct dt_dentry *dn = d;
    LOG_ASSERT((wk != NULL) && (d != NULL), "Bad arguments\n");
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
    LOG_ASSERT((wk != NULL) && (d != NULL), "Bad arguments\n");
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
    LOG_ASSERT(d != NULL, "Bad arguments\n");
    if ((dc = dt_find_dir_sibling(d)) != NULL)
        return dc;
    else
        return d->parent;
}

void dt_mktree(struct dt_walker *wk, struct dt_dentry *root, void *curdir, dt_out out)
{
    struct dt_dentry *d, *dc;
    unsigned int id = 1;

    if ((root == NULL) || (wk == NULL)) {
        LOG_ERR("Bad arguments\n");
        return;
    }

    root->parent = NULL;
    root->sibling = NULL;
    root->child = NULL;
    root->stamp = 0;
    root->id = id++;

    if (wk->init(curdir) < 0)
        return;

    dt_readdir(wk, root, curdir, &id, out);

    d = dt_go_child(wk, root, curdir);

    // invariants:
    // d->type == DT_DIR
    // if v->stamp = 1 then descendants of v are processed
    while((d != NULL) && (d != root)) {
        if (d->stamp == 0) {
            dt_readdir(wk, d, curdir, &id, out);
            if ((dc = dt_go_child(wk, d, curdir)) != NULL) {
                d->stamp = 1;
                d = dc;
                continue;
            }
        } else {
            d->stamp = 0;
        }
        for (dc = d->child; dc != NULL; dc = dc->sibling)
            d->size += dc->size;
        d = dt_go_sibling_or_parent(wk, d, curdir);
    }

    for (dc = root->child; dc != NULL; dc = dc->sibling)
        root->size += dc->size;

    wk->fini(curdir);
}

static void dt_printpath(struct dt_dentry *d)
{
    LOG_ASSERT(d != NULL, "Bad arguments\n");
    if (d->parent != NULL) {
        dt_printpath(d->parent);
        printf("/");
    }
    printf("%s",d->name);
}

static void dt_printfile(struct dt_dentry *d, dt_out out)
{
    switch (out) {
        case DT_OUT_FULL:
        default:
            printf("%s %llu\n", (d->type == DT_DIR) ? "/" : "", d->size);
            break;
        case DT_OUT_SIMPLIFIED:
        case DT_OUT_REVERSE:
            printf("%u %u %llu %s\n",
                    (d->parent != NULL) ? d->parent->id : 0,
                    (d->type == DT_DIR) ? d->id : 0,
                    d->size, d->name);
            break;
    }
}

static void dt_printdir(struct dt_dentry *d, dt_out out)
{
    struct dt_dentry *dc;
    LOG_ASSERT(d != NULL, "Bad arguments\n");
    switch (out) {
        case DT_OUT_FULL:
        default:
            for (dc = d->child; dc != NULL; dc = dc->sibling) {
                dt_printpath(dc);
                dt_printfile(dc, out);
            }
            break;
        case DT_OUT_SIMPLIFIED:
        case DT_OUT_REVERSE:
            for (dc = d->child; dc != NULL; dc = dc->sibling) {
                dt_printfile(dc, out);
            }
            break;
    }
}
    
void dt_printtree(struct dt_dentry *root, dt_out out)
{
    struct dt_dentry *d, *dc;

    if (root == NULL) {
        LOG_ERR("bad arguments\n");
        return;
    }

    dt_printfile(root, out);
    dt_printdir(root, out);
    
    d = dt_find_dir_child(root);
    if (d == NULL)
        return;

    while (d != root) {
        if (d->stamp == 0) {
            dt_printdir(d, out);
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
    LOG_ASSERT(d != NULL, "Bad arguments\n");
    dc = d->child;
    while (dc != NULL) {
        dn = dc->sibling;
        free(dc->name);
        free(dc);
        dc = dn;
    }
    d->child = NULL;
}

void dt_free(struct dt_dentry *root)
{
    struct dt_dentry *d, *dc;

    if (root == NULL) {
        LOG_ERR("bad arguments\n");
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

void dt_singlewalk(struct dt_walker *wk, struct dt_dentry *root, void *curdir, dt_out out)
{
    struct dt_dentry *d, *dc;
    unsigned int id = 1;

    if ((root == NULL) || (wk == NULL)) {
        LOG_ERR("Bad arguments\n");
        return;
    }

    root->parent = NULL;
    root->sibling = NULL;
    root->child = NULL;
    root->stamp = 0;
    root->id = id++;

    if (wk->init(curdir) < 0)
        return;

    dt_readdir(wk, root, curdir, &id, out);
    d = dt_go_child(wk, root, curdir);

    while((d != NULL) && (d != root)) {
        if (d->stamp == 0) {
            dt_readdir(wk, d, curdir, &id, out);
            if ((dc = dt_go_child(wk, d, curdir)) != NULL) {
                d->stamp = 1;
                d = dc;
                continue;
            }
        } else {
            d->stamp = 0;
        }
        for (dc = d->child; dc != NULL; dc = dc->sibling)
            d->size += dc->size;
        dt_printdir(d, out);
        dt_free_childs(d);
        d = dt_go_sibling_or_parent(wk, d, curdir);
    }

    for (dc = root->child; dc != NULL; dc = dc->sibling)
        root->size += dc->size;
        
    dt_printdir(root, out);
    dt_free_childs(root);
    dt_printfile(root, out);

    wk->fini(curdir);
}

