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
#include "dtread.h"

struct dt_dentry * dt_alloc()
{
    return (struct dt_dentry *) calloc(1, sizeof(struct dt_dentry));
}

void dt_free(struct dt_dentry *d)
{
    free(d->name);
    free(d->hash);
    free(d);
}

/*
static struct dt_ctx * dt_ctx_alloc()
{
    struct dt_ctx *dc;

    dc = (struct dt_ctx *) calloc(1, sizeof(struct dt_ctx));
    if (dc == NULL)
        LOG_ERR("calloc() returned NULL\n");
    return dc;
}

static void dt_ctx_free(struct dt_ctx *dc)
{
    free(dc);
}

static void dt_ctx_rfree(struct dt_ctx *dc)
{
    dt_rfree(dc->d);
    free(dc->d);
    free(dc);
}
*/

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
static void dt_list_call_sum_free(struct dt_wctx *wc, struct dt_dentry *d, struct dt_dentry **c, void (*callie) (struct dt_wctx *wc, struct dt_dentry *))
{
    struct dt_dentry *dp = *c, *dn;
    LOG_ASSERT((d != NULL) && (c != NULL), "bad arguments\n");
    *c = NULL;
    for (; dp != NULL; dp = dn) {
        dn = dp->sibling;
        callie(wc, dp);
        d->size += dp->size;
        dt_free(dp);
    }
}

static void dt_list_call_sum(struct dt_wctx *wc, struct dt_dentry *d, struct dt_dentry **c, void (*callie) (struct dt_wctx *wc, struct dt_dentry *))
{
    struct dt_dentry *dp = *c, *dn;
    LOG_ASSERT((d != NULL) && (c != NULL), "bad arguments\n");
    for (; dp != NULL; dp = dn) {
        dn = dp->sibling;
        callie(wc, dp);
        d->size += dp->size;
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

static void dt_list_call_free(struct dt_wctx *wc, struct dt_dentry **c, void (*callie) (struct dt_wctx *wc, struct dt_dentry *))
{
    struct dt_dentry *dp = *c, *dn;
    LOG_ASSERT(c != NULL, "Bad arguments\n");
    *c = NULL;
    for (; dp != NULL; dp = dn) {
        dn = dp->sibling;
        callie(wc, dp);
        dt_free(dp);
    }
}

static void dt_list_call(struct dt_wctx *wc, struct dt_dentry **c, void (*callie) (struct dt_wctx *wc, struct dt_dentry *))
{
    struct dt_dentry *dp = *c, *dn;
    LOG_ASSERT(c != NULL, "Bad arguments\n");
    for (; dp != NULL; dp = dn) {
        dn = dp->sibling;
        callie(wc, dp);
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
    struct dt_dentry *dp, *ds, *dps;
    struct umd5_ctx ctx;
    unsigned int m = 0;
    
    if ((d->hash = (char*)malloc(UMD5_VALUE_SIZE*sizeof(char))) == NULL) {
        LOG_ERR("malloc() returned NULL\n");
        return 0;
    }
    
    umd5_init(&ctx);
    dt_list_call1(&(d->child), dt_umd5_update, (void *) &ctx);
    dt_list_call1(&(d->file_child), dt_umd5_update, (void *) &ctx);
    umd5_finish(&ctx);
    umd5_value(&ctx, d->hash);

    for (dp = d->parent; dp != NULL; dp = dp->parent) {
        ds = d;
        dps = dp;
        m = 0;
        
        while ((dps != NULL) && (!umd5_cmpval(ds->hash, dps->hash))) {
            m += ds->items;
            if (m >= DT_RECURSION_THRESHOLD) {
                LOG_ERR("Recursion detected at directory '%s' (id %u). "
                        "Assuming link '%s' (id %u) -> '%s' (id %u)\n",
                    d->name, d->id, ds->name, ds->id, dps->name, dps->id);
                return 1;
            }
            ds = ds->parent;
            dps = dps->parent;
        }
    }
    return 0;
}


static void dt_readdir(struct dt_walker *wk, struct dt_dentry *d, void *curdir, unsigned int *id)
{
    struct dt_dentry *ddc = NULL, *dfc = NULL, *dn;
    unsigned int dirs = 0;
    unsigned int files = 0;
    unsigned int oldid = 0;
    LOG_ASSERT((wk != NULL) && (d != NULL), "Bad arguments\n");

    if (dt_exceed_macro(d->id, DT_MAX_DIRS, d))
        return;

    if (id != NULL)
        oldid = *id;

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
        if (id != NULL)
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

static void dt_printfile_full(struct dt_wctx *wc, struct dt_dentry *d)
{
    dt_printpath(d);
    printf("%s%s %llu\n", wc->prefix,
        ((d->type & DT_TYPE_MASK) == DT_DIR) ? "/" : "", d->size);
}

static void dt_printdir_reverse(struct dt_wctx *wc, struct dt_dentry *d)
{
    printf("%s0 %u ", wc->prefix, d->id);
    dt_printpath(d);
    printf("\n");
}

static void dt_printdir_once_custom(struct dt_wctx *wc, struct dt_dentry *d)
{
    if (!(d->type & DT_TYPE_ONCE)) {
        d->type |= DT_TYPE_ONCE;
        wc->prefix = "* ";
        wc->call_dir(wc, d);
    }
}

static void dt_printdir_once(struct dt_wctx *wc)
{
    dt_printdir_once_custom(wc, wc->d);
}

static void dt_printfile_reverse(struct dt_wctx *wc, struct dt_dentry *d)
{
    printf("%s1 %u %u %llu %u %u ",
            wc->prefix,
            (d->parent != NULL) ? d->parent->id : 0,
            d->fid, d->size,
            ((d->type & DT_TYPE_MASK) == DT_DIR) ? d->id : 0,
            ((d->type & DT_TYPE_MASK) == DT_DIR) ? d->items : 0
            );
    printf("%s", d->name);
    printf("\n");
}

void dt_walk(struct dt_wctx *wc)
{
    struct dt_dentry *root;
    int enter;

    LOG_ASSERT((wc != NULL) && (wc->d != NULL), "Bad arguments\n");

    root = wc->d;
    wc->on_enter_root(wc);

    for (enter = 1; wc->d != NULL;) {
        if (enter) {
            wc->on_enter(wc);
            if (wc->go_child(wc) != 0)
                continue;
        }
        wc->on_leave(wc);
        if (wc->d == root)
            break;
        enter = wc->go_sibling_or_parent(wc);
    }

    wc->d = root;
    wc->on_leave_root(wc);
}

static void dt_wctx_plug(struct dt_wctx *wc)
{
}

static void dt_on_er_init(struct dt_wctx *wc)
{
    dt_init_root(wc->d, &wc->id);
}
static void dt_on_er_print(struct dt_wctx *wc)
{
    wc->call_dir(wc, wc->d);
}
static void dt_on_er_init_print(struct dt_wctx *wc)
{
    dt_on_er_init(wc);
    dt_on_er_print(wc);
}

static void dt_on_e_read(struct dt_wctx *wc)
{
    dt_readdir(wc->wk, wc->d, wc->curdir, &wc->id);
}
static void dt_on_e_print(struct dt_wctx *wc)
{
    dt_list_call(wc, &(wc->d->child), wc->call_dir);
    dt_list_call_sum(wc, wc->d, &(wc->d->file_child), wc->call_file);
}
static void dt_on_e_print_free(struct dt_wctx *wc)
{
    dt_list_call(wc, &wc->d->child, wc->call_dir);
    dt_list_call_free(wc, &wc->d->file_child, wc->call_file);
}
static void dt_on_e_read_print(struct dt_wctx *wc)
{
    dt_on_e_read(wc);
    dt_on_e_print(wc);
}
static void dt_on_e_read_print_sum_free(struct dt_wctx *wc)
{
    dt_on_e_read(wc);
    dt_list_call(wc, &(wc->d->child), wc->call_dir);
    dt_list_call_sum_free(wc, wc->d, &(wc->d->file_child), wc->call_file);
}
static void dt_on_e_dprint_free(struct dt_wctx *wc)
{
    dt_list_call(wc, &(wc->d->child), wc->call_dir);
    dt_list_free(&wc->d->file_child);
}
static void dt_on_e_free(struct dt_wctx *wc)
{
    dt_list_free(&wc->d->file_child);
}

static void dt_on_lr_print(struct dt_wctx *wc)
{
    wc->call_file(wc, wc->d);
}

static void dt_on_l_print_sum_free(struct dt_wctx *wc)
{
    dt_list_call_sum_free(wc, wc->d, &(wc->d->child), wc->call_file);
}
static void dt_on_l_print_free(struct dt_wctx *wc)
{
    dt_list_call_free(wc, &wc->d->child, wc->call_file);
}
static void dt_on_l_print(struct dt_wctx *wc)
{
    dt_list_call_sum(wc, wc->d, &(wc->d->child), wc->call_file);
}
static void dt_on_l_free (struct dt_wctx *wc)
{
    dt_list_free(&wc->d->child);
}
static void dt_on_l_sum(struct dt_wctx *wc)
{
    dt_list_sum(wc->d, &wc->d->child);
    dt_list_sum(wc->d, &wc->d->file_child);
}

static int dt_go_c_walker(struct dt_wctx *wc)
{
    struct dt_dentry *d;
    if ((d = dt_go_child(wc->wk, wc->d, wc->curdir)) != NULL) {
        wc->d = d;
        return 1;
    }
    return 0;
}
static int dt_go_c_ds(struct dt_wctx *wc)
{
    struct dt_dentry *d;
    if ((d = wc->d->child) != NULL) {
        wc->d = d;
        return 1;
    }
    return 0;
}

static int dt_go_sop_walker(struct dt_wctx *wc)
{
    int ret = 0;
    wc->d = dt_go_sibling_or_parent(wc->wk, wc->d, wc->curdir, &ret);
    return ret;
}
static int dt_go_sop_ds(struct dt_wctx *wc)
{
    int ret;
    wc->d = dt_next_sibling_or_parent(wc->d, &ret);
    return ret;
}

void dt_full(struct dt_walker *wk, struct dt_dentry *root, void *curdir)
{
    struct dt_wctx wc;

    if ((root == NULL) || (wk == NULL)) {
        LOG_ERR("Bad arguments\n");
        return;
    }

    wc.d                      = root;
    wc.wk                     = wk;
    wc.curdir                 = curdir;
    wc.on_enter_root          = dt_on_er_init;
    wc.on_leave_root          = dt_wctx_plug;
    wc.on_enter               = dt_on_e_read;
    wc.on_leave               = dt_on_l_sum;
    wc.go_child               = dt_go_c_walker;
    wc.go_sibling_or_parent   = dt_go_sop_walker;
    wc.call_dir               = dt_printfile_full;
    wc.call_file              = dt_printfile_full;
    wc.prefix                 = "";
    wc.id                     = 1;

    dt_walk(&wc);

    wc.on_enter_root    = dt_on_er_print;
    wc.on_enter         = dt_on_e_print_free;
    wc.on_leave         = dt_on_l_free;
    wc.go_child               = dt_go_c_ds,
    wc.go_sibling_or_parent   = dt_go_sop_ds,

    dt_walk(&wc);
}


void dt_reverse(struct dt_walker *wk, struct dt_dentry *root, void *curdir)
{
    struct dt_wctx wc;

    if ((root == NULL) || (wk == NULL)) {
        LOG_ERR("Bad arguments\n");
        return;
    }

    wc.d                      = root;
    wc.wk                     = wk;
    wc.curdir                 = curdir;
    wc.on_enter_root          = dt_on_er_init_print;
    wc.on_leave_root          = dt_on_lr_print;
    wc.on_enter               = dt_on_e_read_print_sum_free;
    wc.on_leave               = dt_on_l_print_sum_free;
    wc.go_child               = dt_go_c_walker;
    wc.go_sibling_or_parent   = dt_go_sop_walker;
    wc.call_dir               = dt_printdir_reverse;
    wc.call_file              = dt_printfile_reverse;
    wc.prefix                 = "";
    wc.id                     = 1;

    dt_walk(&wc);
}

void dt_reverse_p(struct dt_dentry *root)
{
    struct dt_wctx wc;

    if (root == NULL) {
        LOG_ERR("Bad arguments\n");
        return;
    }

    wc.d                      = root;
    wc.on_enter_root          = dt_on_er_print;
    wc.on_leave_root          = dt_on_lr_print;
    wc.on_enter               = dt_on_e_print_free;
    wc.on_leave               = dt_on_l_print_free;
    wc.go_child               = dt_go_c_ds;
    wc.go_sibling_or_parent   = dt_go_sop_ds;
    wc.call_dir               = dt_printdir_reverse;
    wc.call_file              = dt_printfile_reverse;
    wc.prefix                 = "";

    dt_walk(&wc);
}

void dt_rfree(struct dt_dentry *root)
{
    struct dt_wctx wc;

    if (root == NULL)
        return;

    wc.d                      = root;
    wc.on_enter_root          = dt_wctx_plug;
    wc.on_leave_root          = dt_wctx_plug;
    wc.on_enter               = dt_on_e_free;
    wc.on_leave               = dt_on_l_free;
    wc.go_child               = dt_go_c_ds;
    wc.go_sibling_or_parent   = dt_go_sop_ds;

    dt_walk(&wc);
    dt_free(root);
}

static void dt_list_diff(struct dt_wctx *wc, struct dt_dentry **o, struct dt_dentry **n)
{
    struct dt_dentry *odp = *o;
    struct dt_dentry *dp = *n;
    int cmp;

    LOG_ASSERT((o != NULL) && (n != NULL), "Bad arguments\n");
    while ((odp != NULL) && (dp != NULL)) {
        if ((cmp = strcmp(odp->name, dp->name)) == 0) {
            if (odp->size != dp->size) {
                dt_printdir_once(wc);
                wc->prefix = "* ";
                wc->call_file(wc, dp);
            }
            dp = dp->sibling;
            odp = odp->sibling;
        } else if (cmp < 0) {
            dt_printdir_once(wc);
            wc->prefix = "- ";
            wc->call_file(wc, odp);
            odp = odp->sibling;
        } else {
            dt_printdir_once(wc);
            wc->prefix = "+ ";
            wc->call_file(wc, dp);
            dp = dp->sibling;
        }
    }
    while (odp != NULL) {
        dt_printdir_once(wc);
        wc->prefix = "- ";
        wc->call_file(wc, odp);
        odp = odp->sibling;
    }
    while (dp != NULL) {
        dt_printdir_once(wc);
        wc->prefix = "+ ";
        wc->call_file(wc, dp);
        dp = dp->sibling;
    }
}

static void dt_diff_delete_tree(struct dt_wctx *pwc, struct dt_dentry *root)
{
    struct dt_wctx wc;
    LOG_ASSERT(root != NULL, "Bad arguments\n");
    wc.d                      = root;
    wc.wk                     = NULL;
    wc.curdir                 = NULL;
    wc.on_enter_root          = dt_on_er_print;
    wc.on_enter               = dt_on_e_dprint_free;
    wc.on_leave_root          = dt_on_lr_print;
    wc.on_leave               = dt_on_l_free;
    wc.go_child               = dt_go_c_ds;
    wc.go_sibling_or_parent   = dt_go_sop_ds;
    wc.call_dir               = dt_printdir_reverse;
    wc.call_file              = dt_printfile_reverse;
    wc.prefix                 = "- ";
    wc.id                     = 1;
    
    dt_walk(&wc);
    dt_free(root);
}

static void dt_diff_add_tree(struct dt_wctx *pwc, struct dt_dentry *root)
{
    struct dt_wctx wc;
    wc.d                      = root;
    wc.wk                     = pwc->wk;
    wc.curdir                 = pwc->curdir;
    wc.on_enter_root          = dt_on_er_print;
    wc.on_enter               = dt_on_e_read_print;
    wc.on_leave_root          = dt_on_lr_print;
    wc.on_leave               = dt_on_l_print;
    wc.go_child               = dt_go_c_walker;
    wc.go_sibling_or_parent   = dt_go_sop_walker;
    wc.call_dir               = dt_printdir_reverse;
    wc.call_file              = dt_printfile_reverse;
    wc.prefix                 = "+ ";
    wc.id                     = pwc->id;

    root->id = wc.id++;

    if (wc.wk->go(DT_GO_CHILD, root->name, wc.curdir) >= 0) {
        dt_walk(&wc);
        wc.wk->go(DT_GO_PARENT, NULL, wc.curdir);
    }

    pwc->id = wc.id;
}

/* diff childs lists. Prints whiole trees for unique childs.
 * Reconstruct old list to be identical */
static void dt_list_diff_childs(struct dt_wctx *wc, struct dt_dentry *d, struct dt_walker *wk, struct dt_dentry **o, struct dt_dentry **n, void *curdir)
{
    struct dt_dentry *odp, *odn = NULL, *ods = NULL;
    struct dt_dentry *dp;
    struct dt_dentry *tmp;
    int cmp;
    LOG_ASSERT((o != NULL) && (n != NULL), "Bad arguments\n");

    odp = *o;
    dp = *n;
    while ((odp != NULL) && (dp != NULL)) {
        if ((cmp = strcmp(odp->name, dp->name)) == 0) {
            if (ods == NULL)
                ods = odp;
            else
                odn->sibling = odp;
            odn = odp;
            dp->id = odp->id;
            dp = dp->sibling;
            odp = odp->sibling;
        } else if (cmp < 0) {
            dt_printdir_once(wc);
            tmp = odp->sibling;
            dt_diff_delete_tree(wc, odp);
            odp = tmp;
        } else {
            dt_printdir_once(wc);
            dp->type |= DT_TYPE_NEW;
            dt_diff_add_tree(wc, dp);
            dp = dp->sibling;
        }
    }
    while (odp != NULL) {
        dt_printdir_once(wc);
        tmp = odp->sibling;
        dt_diff_delete_tree(wc, odp);
        odp = tmp;
    }
    while (dp != NULL) {
        dt_printdir_once(wc);
        dp->type |= DT_TYPE_NEW;
        dt_diff_add_tree(wc, dp);
        dp = dp->sibling;
    }
    if (odn != NULL)
        odn->sibling = NULL;
    *o = ods;
}

static void dt_on_er_diff(struct dt_wctx *wc)
{
    wc->d->parent = NULL;
    wc->d->sibling = NULL;
    wc->d->child = NULL;
    wc->d->file_child = NULL;
    wc->d->hash = NULL;
    wc->d->id = wc->od->id;
}

static void dt_on_e_diff(struct dt_wctx *wc)
{
    dt_readdir(wc->wk, wc->d, wc->curdir, NULL);
    dt_list_sum(wc->d, &wc->d->file_child);
    dt_list_diff_childs(wc, wc->d, wc->wk, &wc->od->child, &wc->d->child, wc->curdir);
    dt_list_diff(wc, &wc->od->file_child, &wc->d->file_child);
}
static void dt_on_l_diff(struct dt_wctx *wc)
{
    dt_list_sum(wc->d, &wc->d->child);
    if((wc->d->size != wc->od->size) || (wc->d->items != wc->od->items)) {
        /* FIXME: this is not needed because action '*' on file doesn't requere
         * it's path */
        //if (wc->d->parent)
        //    dt_printdir_once_custom(wc, wc->d->parent);
        wc->prefix = "* ";
        wc->call_file(wc, wc->d);
    }
}
static int dt_go_c_diff(struct dt_wctx *wc)
{
    struct dt_dentry *d, *od, *dc;
    LOG_ASSERT(wc != NULL, "Bad arguments\n");
    
    if ((dc = wc->d->child) == NULL)
        return 0;
    
    d = wc->d;
    od = wc->od;
    wc->d = wc->d->child;
    wc->od = wc->od->child;

    while (dc != NULL) {
        if (dc->type & DT_TYPE_NEW) {
            dc = dc->sibling;
            wc->d = dc;
            continue;
        }
        if (wc->wk->go(DT_GO_CHILD, dc->name, wc->curdir) < 0) {
            dt_list_diff_childs(wc, wc->d, wc->wk, &wc->od->child, &wc->d->child, wc->curdir);
            dt_list_diff(wc, &wc->od->file_child, &wc->d->file_child);
            dt_on_l_diff(wc);
            dc = dc->sibling;
            wc->d = dc;
            wc->od = wc->od->sibling;
            continue;
        }
        break;
    }
    
    if (dc == NULL) {
        wc->d = d;
        wc->od = od;
    }
    return (dc != NULL);
}

static int dt_go_sop_diff(struct dt_wctx *wc)
{
    int ret;
    struct dt_dentry *dn, *parent;
    LOG_ASSERT((wc != NULL) && (wc->wk != NULL) && (wc->d != NULL), "Bad arguments\n");

    dn = wc->d;
    wc->d = wc->d->parent;
    parent = wc->d;
    if (wc->d == NULL)
        return 0;

    if (wc->wk->go(DT_GO_PARENT, NULL, wc->curdir) < 0) {
        LOG_ERR("Unable to return to parent directory.\n"); 
        exit(ESTAT_FAILURE);
    }

    while ((dn = dn->sibling) != NULL) {
        if (dn->type & DT_TYPE_NEW)
            continue;
        wc->od = wc->od->sibling;
        wc->d = dn;
        if (wc->wk->go(DT_GO_CHILD, dn->name, wc->curdir) < 0) {
#if 0
            dt_printdir_once(wc);
            dt_printdir_once_custom(wc, dn);
            dt_list_call(wc, &wc->od->child, dt_diff_delete_tree);
            wc->prefix = "- ";
            dt_list_call_free(wc, &wc->od->file_child, wc->call_file);
            wc->prefix = "* ";
            wc->call_file(wc, dn);
#else       
            dt_list_diff_childs(wc, wc->d, wc->wk, &wc->od->child, &wc->d->child, wc->curdir);
            dt_list_diff(wc, &wc->od->file_child, &wc->d->file_child);
            dt_on_l_diff(wc);
#endif
            continue;
        }
        break;
    }

    ret = (dn != NULL);
    if (!ret) {
        wc->d = parent;
        wc->od = wc->od->parent;
    }
    return ret;

}

void dt_diff(FILE *file, struct dt_walker *wk, struct dt_dentry *root, void *curdir)
{
    struct dt_wctx wc;
    struct dt_dentry *oldroot;
    wc.d                      = root;
    wc.wk                     = wk;
    wc.curdir                 = curdir;
    wc.on_enter_root          = dt_on_er_diff;
    wc.on_enter               = dt_on_e_diff;
    wc.on_leave_root          = dt_wctx_plug;
    wc.on_leave               = dt_on_l_diff;
    wc.go_child               = dt_go_c_diff;
    wc.go_sibling_or_parent   = dt_go_sop_diff;
    wc.call_dir               = dt_printdir_reverse;
    wc.call_file              = dt_printfile_reverse;
    wc.id                     = 0;
    
    if ((file == NULL) || (root == NULL) || (wk == NULL)) {
        LOG_ERR("Bad arguments\n");
        return;
    }

    if ((oldroot = dtread_readfile(file, &wc.id)) == NULL) {
        /* currently external program cannot distinguish empty patch
         * from failed tree restore, so we have to abort here */
        LOG_ASSERT(0, "Tree reconstruction failed");
        //dt_reverse(wk, root, curdir);
        return;
    }

    wc.od = oldroot;
    wc.id++;
    
    dt_walk(&wc);

    dt_rfree(oldroot);
    dt_reverse_p(root);
}

