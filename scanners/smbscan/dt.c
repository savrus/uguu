/* dt.h - 'dir tree' engine
 * Walks directory subtree and counts size like du(1).
 * File system navigation functions as well as readdir are
 * provided externally through dt_walker structure
 *
 * Copyright 2009, 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "dt.h"
#include "log.h"

static void dt_printfile(struct dt_dentry *d, dt_out out);

int dt_compare(const void *dd1, const void *dd2)
{
    struct dt_dentry *d1 = *(struct dt_dentry **) dd1;
    struct dt_dentry *d2 = *(struct dt_dentry **) dd2;
    return strcmp(d1->name, d2->name);
}

/* sorts list of dt_detnries by names and
 * assignes fids according to the order starting from s */
static struct dt_dentry * dt_list_sort(struct dt_dentry *d, size_t nmemb, unsigned int s)
{
    struct dt_dentry **da;
    struct dt_dentry *dp;
    int i;
    LOG_ASSERT((d != NULL) && (nmemb > 0), "Bad arguments\n");

    da = (struct dt_dentry **) malloc(nmemb * sizeof(struct dt_dentry *));
    for (da[0] = d, i = 1; i < nmemb; i++)
        da[i] = da[i-1]->sibling;
    qsort (da, nmemb, sizeof(struct dt_dentry *), dt_compare);
    for (i = 1, dp = da[0], dp->fid = s++; i < nmemb; i++) {
        dp->sibling = da[i];
        dp = da[i];
        dp->fid = s++;
    }
    dp->sibling = NULL;
    dp = da[0];
    free(da);
    return dp;
}

static void dt_list_print_and_free(struct dt_dentry *d, dt_out out)
{
    struct dt_dentry *dn;
    for (; d != NULL; d = dn) {
        dn = d->sibling;
        dt_printfile(d, out);
        free(d->name);
        free(d);
    }
}

/* link next read child into dir tree according to selectd out policy
 * d - current directory
 * dp - last linked child (NULL if none)
 * dn - newly read child
 *
 * return: dn, if dn linked, dc otherwise
 */
static struct dt_dentry * dt_linkchild(struct dt_dentry *d, struct dt_dentry *dp, struct dt_dentry *dn)
{
    LOG_ASSERT((d != NULL) && (dn != NULL), "Bad arguments\n");
    dn->parent = d;
    if (dp == NULL)
        if (dn->type == DT_DIR)
            d->child = dn;
        else
            d->file_child = dn;
    else
        dp->sibling = dn;
    if (dn->type == DT_FILE)
        d->size += dn->size;
    return dn;
}

static void dt_readdir(struct dt_walker *wk, struct dt_dentry *d, void *curdir, unsigned int *id, dt_out out)
{
    struct dt_dentry *ddc = NULL, *dfc = NULL, *dn;
    unsigned int dirs = 0;
    unsigned int files = 0;
    LOG_ASSERT((wk != NULL) && (d != NULL), "Bad arguments\n");
    
    d->total = 0;
    while ((dn = wk->readdir(curdir)) != NULL) {
        dn->stamp = 0;
        if (dn->type == DT_DIR) {
            dirs++;
            dn->id = (*id)++;
            ddc = dt_linkchild(d, ddc, dn);
        } else {
            files++;
            dfc = dt_linkchild(d, dfc, dn);
        }
    }
    if (ddc != NULL)
        ddc->sibling = NULL;
    if (dfc != NULL)
        dfc->sibling = NULL;

    if (dirs > 0)
        d->child = dt_list_sort(d->child, dirs, 0);
    if (files > 0)
        d->file_child = dt_list_sort(d->file_child, files, dirs);
    d->total = files + dirs;

    if ((out == DT_OUT_REVERSE) || (out == DT_OUT_SIMPLIFIED)) {
        dfc = d->file_child;
        d->file_child = NULL;
        dt_list_print_and_free(dfc, out);
    }
}

static struct dt_dentry * dt_find_dir_sibling(struct dt_dentry *d)
{
    LOG_ASSERT((d != NULL) && (d->type == DT_DIR), "Bad arguments\n");
    if (d->sibling != NULL)
        LOG_ASSERT(d->sibling->type == DT_DIR, "Not DT_DIR node in DIR tree\n");
    return d->sibling;
}

static struct dt_dentry * dt_find_dir_child(struct dt_dentry *d)
{
    LOG_ASSERT(d != NULL, "Bad arguments\n");
    if (d->child != NULL)
        LOG_ASSERT(d->child->type == DT_DIR, "Not DT_DIR node in DIR tree\n"); 
    return d->child;
}

static struct dt_dentry * dt_go_sibling_or_parent(struct dt_walker *wk, struct dt_dentry *d, void *curdir)
{
    struct dt_dentry *dn = d;
    LOG_ASSERT((wk != NULL) && (d != NULL), "Bad arguments\n");
    while (((dn = dt_find_dir_sibling(dn)) != NULL)
           && (wk->go(DT_GO_SIBLING, dn->name, curdir) < 0))
           ;
    if (dn == NULL) {
        dn = d->parent;
        if (wk->go(DT_GO_PARENT, NULL, curdir) < 0)
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
    while ((wk->go(DT_GO_CHILD, dc->name, curdir) < 0)
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
    if ((d->parent != NULL) && (d->parent->name[0] != 0) ) {
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
            printf("%u %u %u %llu ",
                    (d->parent != NULL) ? d->parent->id : 0,
                    (d->type == DT_DIR) ? d->id : 0,
                    d->fid, d->size);
            if (d->type == DT_DIR)
                dt_printpath(d);
            else
                printf("%s", d->name);
            printf("\n");
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
            for (dc = d->file_child; dc != NULL; dc = dc->sibling) {
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
    root->file_child = NULL;
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

