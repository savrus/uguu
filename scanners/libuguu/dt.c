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
#include "estat.h"
#include "umd5.h"

struct dt_dentry * dt_alloc()
{
    return (struct dt_dentry *) calloc(1, sizeof(struct dt_dentry));
}

void dt_free(struct dt_dentry *d)
{
    free(d->name);
    if (d->hash != NULL)
        free(d->hash);
    free(d);
}

static void dt_init_root(struct dt_dentry *root, unsigned int *id)
{
    root->parent = NULL;
    root->sibling = NULL;
    root->child = NULL;
    root->file_child = NULL;
    root->hash = NULL;
    root->id = (*id)++;
}

int dt_compare(const void *dd1, const void *dd2)
{
    struct dt_dentry *d1 = *(struct dt_dentry **) dd1;
    struct dt_dentry *d2 = *(struct dt_dentry **) dd2;
    return strcmp(d1->name, d2->name);
}

/* sorts list of dt_detnries by names and
 * assignes fids according to the order starting from s */
static struct dt_dentry * dt_list_sort(struct dt_dentry *d, size_t nmemb, unsigned int s, unsigned int *id)
{
    struct dt_dentry **da;
    struct dt_dentry *dp;
    size_t i;
    LOG_ASSERT((d != NULL) && (nmemb > 0), "Bad arguments\n");

    da = (struct dt_dentry **) malloc(nmemb * sizeof(struct dt_dentry *));
    for (da[0] = d, i = 1; i < nmemb; i++)
        da[i] = da[i-1]->sibling;
    qsort (da, nmemb, sizeof(struct dt_dentry *), dt_compare);
    for (i = 1, dp = da[0], dp->fid = s++,
            dp->id = (id != NULL) ? (*id)++ : 0; i < nmemb; i++) {
        dp->sibling = da[i];
        dp = da[i];
        dp->id = (id != NULL) ? (*id)++ : 0;
        dp->fid = s++;
    }
    dp->sibling = NULL;
    dp = da[0];
    free(da);
    return dp;
}

/* print child list, add sizes to parent and free childs
 * d - parent
 * c - pointer to child list pointer in d structure
 */
static void dt_list_call_sum_free(struct dt_dentry *d, struct dt_dentry **c, void (*callie) (struct dt_dentry *))
{
    struct dt_dentry *dp = *c, *dn;
    LOG_ASSERT((d != NULL) && (c != NULL), "Bad arguments\n");
    *c = NULL;
    for (; dp != NULL; dp = dn) {
        dn = dp->sibling;
        callie(dp);
        d->size += dp->size;
        dt_free(dp);
    }
}

static void dt_list_sum(struct dt_dentry *d, struct dt_dentry **c)
{
    struct dt_dentry *dp = *c, *dn;
    LOG_ASSERT((d != NULL) && (c != NULL), "Bad arguments\n");
    for (; dp != NULL; dp = dn) {
        dn = dp->sibling;
        d->size += dp->size;
    }
}

static void dt_list_call_free(struct dt_dentry **c, void (*callie) (struct dt_dentry *))
{
    struct dt_dentry *dp = *c, *dn;
    LOG_ASSERT(c != NULL, "Bad arguments\n");
    *c = NULL;
    for (; dp != NULL; dp = dn) {
        dn = dp->sibling;
        callie(dp);
        dt_free(dp);
    }
}

static void dt_list_call(struct dt_dentry **c, void (*callie) (struct dt_dentry *))
{
    struct dt_dentry *dp = *c, *dn;
    LOG_ASSERT(c != NULL, "Bad arguments\n");
    for (; dp != NULL; dp = dn) {
        dn = dp->sibling;
        callie(dp);
    }
}

static void dt_list_call1(struct dt_dentry **c, void (*callie) (struct dt_dentry *, void *arg), void *arg)
{
    struct dt_dentry *dp = *c, *dn;
    LOG_ASSERT(c != NULL, "Bad arguments\n");
    for (; dp != NULL; dp = dn) {
        dn = dp->sibling;
        callie(dp, arg);
    }
}

static void dt_list_free(struct dt_dentry **c)
{
    struct dt_dentry *dp = *c, *dn;
    LOG_ASSERT(c != NULL, "Bad arguments\n");
    *c = NULL;
    for (; dp != NULL; dp = dn) {
        dn = dp->sibling;
        dt_free(dp);
    }
}

/* link next read child into dir tree according to selectd out policy
 * c - pointer to list in current directory structure
 * dp - last linked child (NULL if none)
 * dn - newly read child
 *
 * return: dn, if dn linked, dc otherwise
 */
static struct dt_dentry * dt_linkchild(struct dt_dentry **c, struct dt_dentry *dp, struct dt_dentry *dn)
{
    LOG_ASSERT((c != NULL) && (dn != NULL), "Bad arguments\n");
    if (dp == NULL)
        *c = dn;
    else
        dp->sibling = dn;
    return dn;
}

static int dt_exceed_limit(unsigned int value, unsigned int limit, char *str, struct dt_dentry *d)
{
    if (value > limit) {
        LOG_ERR("%s limit exceeded for directory '%s' (id %u). "
                "Increase %s and recompile if you want to scan everything.",
                str, d->name, d->id, str);
        return 1;
    }           
    return 0;
}

#define dt_exceed_macro(value, limit, d) \
    dt_exceed_limit(value, limit, #limit, d)

static void dt_umd5_update(struct dt_dentry *d, void *arg)
{
    struct umd5_ctx *ctx = (struct umd5_ctx *) arg;
    umd5_update(ctx, d->name, strlen(d->name) + 1);
    umd5_update(ctx, (char *) &d->size, sizeof(d->size));
}

/* return: 1 if recursion detected, 0 otherwise */
static int dt_recursive_link(struct dt_dentry *d)
{
    struct dt_dentry *dp;
    struct umd5_ctx ctx;

    if (d->items > DT_RECURSION_THRESHOLD) {
        umd5_init(&ctx);
        dt_list_call1(&(d->child), dt_umd5_update, (void *) &ctx);
        dt_list_call1(&(d->file_child), dt_umd5_update, (void *) &ctx);
        umd5_finish(&ctx);

        if ((d->hash = (char*)malloc(UMD5_VALUE_SIZE*sizeof(char))) == NULL) {
            LOG_ERR("malloc() returned NULL\n");
            return 0;
        }

        umd5_value(&ctx, d->hash);

        for (dp = d->parent; dp != NULL; dp = dp->parent) {
            if ((dp->hash != NULL) && (!umd5_cmpval(d->hash, dp->hash))) {
                LOG_ERR("Recursion detected at directory '%s' (id %u). "
                        "Assuming link target '%s' (id %u)\n",
                    d->name, d->id, dp->name, dp->id);
                return 1;
            }
        }
    }
    return 0;
}


static void dt_readdir(struct dt_walker *wk, struct dt_dentry *d, void *curdir, unsigned int *id)
{
    struct dt_dentry *ddc = NULL, *dfc = NULL, *dn;
    unsigned int dirs = 0;
    unsigned int files = 0;
    unsigned int oldid = *id;
    LOG_ASSERT((wk != NULL) && (d != NULL), "Bad arguments\n");

    if (dt_exceed_macro(d->id, DT_MAX_DIRS, d))
        return;

    while ((dn = wk->readdir(curdir)) != NULL) {
        if (dt_exceed_macro(files + dirs + 1, DT_MAX_ITEMS_IN_DIR, d)) {
            dt_free(dn);
            break;
        }
        dn->parent = d;
        if (dn->type == DT_DIR) {
            dirs++;
            ddc = dt_linkchild(&(d->child), ddc, dn);
        } else {
            files++;
            dfc = dt_linkchild(&(d->file_child), dfc, dn);
        }
    }
    if (ddc != NULL)
        ddc->sibling = NULL;
    if (dfc != NULL)
        dfc->sibling = NULL;

    if (dirs > 0)
        d->child = dt_list_sort(d->child, dirs, 0, id);
    if (files > 0)
        d->file_child = dt_list_sort(d->file_child, files, dirs, NULL);
    d->items = files + dirs;

    if (dt_recursive_link(d)) {
        *id = oldid;
        d->items = 0;
        dt_list_free(&(d->child));
        dt_list_free(&(d->file_child));
    }
}

static struct dt_dentry * dt_go_sibling_or_parent(struct dt_walker *wk, struct dt_dentry *d, void *curdir, int *went_to_sibling)
{
    struct dt_dentry *dn = d;
    LOG_ASSERT((wk != NULL) && (d != NULL), "Bad arguments\n");

    if (d->parent == NULL)
        return NULL;

    if (wk->go(DT_GO_PARENT, NULL, curdir) < 0) {
        LOG_ERR("Unable to return to parent directory.\n"); 
        exit(ESTAT_FAILURE);
    }

    while (((dn = dn->sibling) != NULL)
           && (wk->go(DT_GO_CHILD, dn->name, curdir) < 0))
        ;

    *went_to_sibling = (dn != NULL);
    return (dn != NULL) ? dn : d->parent;
}

static struct dt_dentry * dt_go_child(struct dt_walker *wk, struct dt_dentry *d, void *curdir)
{
    struct dt_dentry *dc;
    LOG_ASSERT((wk != NULL) && (d != NULL), "Bad arguments\n");
    if ((dc = d->child) == NULL)
        return NULL;
    while ((wk->go(DT_GO_CHILD, dc->name, curdir) < 0)
           && ((dc = dc->sibling) != NULL))
        ;
    return dc;
}

static struct dt_dentry * dt_next_sibling_or_parent(struct dt_dentry *d, int *went_to_sibling)
{
    LOG_ASSERT(d != NULL, "Bad arguments\n");
    *went_to_sibling = (d->sibling != NULL);
    return (d->sibling != NULL) ? d->sibling : d->parent;
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

static void dt_printfile_full(struct dt_dentry *d)
{
    dt_printpath(d);
    printf("%s %llu\n", (d->type == DT_DIR) ? "/" : "", d->size);
}

void dt_full(struct dt_walker *wk, struct dt_dentry *root, void *curdir)
{
    struct dt_dentry *d, *dc;
    unsigned int id = 1;
    int enter;

    if ((root == NULL) || (wk == NULL)) {
        LOG_ERR("Bad arguments\n");
        return;
    }

    dt_init_root(root, &id);

    for (d = root, enter = 1; d != NULL;) {
        if (enter) {
            dt_readdir(wk, d, curdir, &id);
            if ((dc = dt_go_child(wk, d, curdir)) != NULL) {
                d = dc;
                continue;
            }
        }
        dt_list_sum(d, &(d->file_child));
        dt_list_sum(d, &(d->child));
        d = dt_go_sibling_or_parent(wk, d, curdir, &enter);
    }

    dt_printfile_full(root);
    for (d = root, enter = 1; d != NULL;) {
        if (enter) {
            dt_list_call(&(d->child), dt_printfile_full);
            dt_list_call_free(&(d->file_child), dt_printfile_full);
            if ((dc = d->child) != NULL) {
                d = dc;
                continue;
            }
        } 
        dt_list_free(&(d->child));
        d = dt_next_sibling_or_parent(d, &enter);
    }
}

static void dt_printdir_reverse(struct dt_dentry *d)
{
    printf("0 %u ", d->id);
    dt_printpath(d);
    printf("\n");
}

static void dt_printfile_reverse(struct dt_dentry *d)
{
    printf("1 %u %u %llu %u %u ",
            (d->parent != NULL) ? d->parent->id : 0,
            d->fid, d->size,
            (d->type == DT_DIR) ? d->id : 0,
            (d->type == DT_DIR) ? d->items : 0
            );
    printf("%s", d->name);
    printf("\n");
}

void dt_reverse(struct dt_walker *wk, struct dt_dentry *root, void *curdir)
{
    struct dt_dentry *d, *dc;
    unsigned int id = 1;
    int enter;

    if ((root == NULL) || (wk == NULL)) {
        LOG_ERR("Bad arguments\n");
        return;
    }

    dt_init_root(root, &id);
    dt_printdir_reverse(root);

    for (d = root, enter = 1; d != NULL;) {
        if (enter) {
            dt_readdir(wk, d, curdir, &id);
            dt_list_call(&(d->child), dt_printdir_reverse);
            dt_list_call_sum_free(d, &(d->file_child), dt_printfile_reverse);
            if ((dc = dt_go_child(wk, d, curdir)) != NULL) {
                d = dc;
                continue;
            }
        }
        dt_list_call_sum_free(d, &(d->child), dt_printfile_reverse);
        d = dt_go_sibling_or_parent(wk, d, curdir, &enter);
    }

    dt_printfile_reverse(root);
}

