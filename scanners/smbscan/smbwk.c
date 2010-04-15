/* smbwk - smb walker for 'dir tree' engine
 *
 * Copyright 2009, 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <libsmbclient.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "dt.h"
#include "estat.h"
#include "smbwk.h"
#include "log.h"
#include "stack.h"
#include "buf.h"

static struct smbwk_urlpath * smbwk_urlpath_alloc()
{
    struct smbwk_urlpath *up;

    up = (struct smbwk_urlpath *) malloc(sizeof(struct smbwk_urlpath));
    if (up == NULL)
        LOG_ERR("malloc() returned NULL\n");
    return up;
}

static void smbwk_urlpath_free(struct stack *s)
{
    struct smbwk_urlpath *up;

    up = stack_data(s, struct smbwk_urlpath, parent);
    free(up);
}

/* appends new file/dir to url
 * return 0 if failed, 1 otherwise */
static int smbwk_url_append(struct smbwk_dir *c, char *name)
{
    struct smbwk_urlpath *up;

    if ((up = smbwk_urlpath_alloc()) == NULL)
        return 0;
    up->urlpos = buf_strlen(c->url);
    stack_push(&c->paths, &up->parent);
    
    buf_appendf(c->url, "/%s", name);
    if (buf_error(c->url))
        return 0;
    return 1;
}

/* suspends the last file/dir from url string */
static void smbwk_url_suspend(struct smbwk_dir *c)
{
    struct smbwk_urlpath *up;
    
    up = stack_data(stack_pop(&c->paths), struct smbwk_urlpath , parent);
    buf_chop(c->url, up->urlpos);
    free(up);
}

void smbwk_auth(const char *srv, 
                const char *shr,
                char *wg, int wglen, 
                char *un, int unlen,
                char *pw, int pwlen)
{
}

int smbwk_open(struct smbwk_dir *c, char *host, int skip_bucks)
{
    if ((c->url = buf_alloc()) == NULL)
        return -1;

    buf_appendf(c->url, "smb://%s", host);

    if (buf_error(c->url))
        return -1;

    c->ctx = smbc_new_context();
    smbc_option_set(c->ctx, "user", "guest");
    //smbc_setOptionUrlEncodeReaddirEntries(c->ctx, 1);

    if (smbc_init_context(c->ctx) != c->ctx) {
        LOG_ERR("smbc_init_context() failed\n");
        buf_free(c->url);
        return -1;
    }

    if (smbc_init(smbwk_auth, 0) != 0) {
        LOG_ERR("smbc_init() failed\n");
        buf_free(c->url);
        smbc_free_context(c->ctx, 1);
        return -1;
    }

    if ((c->fd = smbc_opendir(buf_string(c->url))) < 0) {
        LOG_ERR("smbc_opendir() failed\n");
        c->fd_real = 0;
        buf_free(c->url);
        smbc_free_context(c->ctx, 1);
        return -1;
    }
    c->fd_real = 1;

    c->skip_bucks = skip_bucks;

    return 1;
}

int smbwk_close(struct smbwk_dir *c)
{
    int ret = 1;    
    
    if (c->fd_real == 1)
        if (smbc_closedir(c->fd) < 0)
            LOG_ERR("smbc_closedir() returned error. url: %s\n",
                buf_string(c->url));
    c->fd_real = 0;

    if (smbc_free_context(c->ctx, 1) == 1) {
        LOG_ERR("smbc_free_context() failed\n");
        ret = -1;
    }

    buf_free(c->url);
    stack_free(&c->paths, smbwk_urlpath_free);
    
    return ret;
}

static int smbwk_skip_bucks(int skip, char *name)
{
    switch (skip) {
        case SKIP_BUCKS_ALL:
            return (strlen(name) > 0 && name[strlen(name) - 1] == '$');
        case SKIP_BUCKS_ADMIN:
            if (((name[0] >= 'A' && name[0] <= 'Z')
                 || (name[0] >= 'a' && name[0] <= 'z'))
                && name[1] == '$' && name[2] == 0)
                return 1;
            if (!strcasecmp(name, "admin$"))
                return 1;
    }
    return 0;
}
            
struct dt_dentry * smbwk_readdir(void *curdir)
{
    struct smbwk_dir *c = (struct smbwk_dir*) curdir;
    struct smbc_dirent *de;
    struct dt_dentry *d;
    struct stat s;
    
    while ((de = smbc_readdir(c->fd)) != NULL) {
        if (!strcmp(de->name,".") || !strcmp(de->name,".."))
           continue;
        if (c->skip_bucks != SKIP_BUCKS_NONE
            && smbwk_skip_bucks(c->skip_bucks, de->name))
            continue;
        if ((de->smbc_type == SMBC_FILE_SHARE)
            || (de->smbc_type == SMBC_DIR)
            || (de->smbc_type == SMBC_FILE))
            break;
    }
    if (de == NULL)
        return NULL;

    d = dt_alloc();
    if (d == NULL) {
        LOG_ERR("dt_alloc() returned NULL\n");
        return NULL;
    }
    d->name = strdup(de->name);
    switch (de->smbc_type) {
        case SMBC_FILE_SHARE:
        case SMBC_DIR:
            d->type = DT_DIR;
            break;
        case SMBC_FILE:
            d->type = DT_FILE;
            if (smbwk_url_append(c, de->name) != 0) {
                if (smbc_stat(buf_string(c->url), &s) == 0)
                    d->size = s.st_size;
                smbwk_url_suspend(c);
            }
            break;
    }
    
    return d;
}

static int smbwk_go(dt_go type, char *name, void *curdir)
{
    struct smbwk_dir *c = (struct smbwk_dir*) curdir;
    int fd = c->fd;
    int fd_real = c->fd_real;

    switch (type) {
        case DT_GO_PARENT:
            smbwk_url_suspend(c);
            fd_real = 0;
            break;
        case DT_GO_CHILD:
            if (smbwk_url_append(c, name) == 0){
                LOG_ERR("smbwk_url_append() returned error. url: %s, append: %s, go_type: %d\n",
                        buf_string(c->url), name, type);
                return -1;
            }

            /* 'dir tree' engine won't request readdir afrer go_parent,
             * so we don't have to call smbc_opendir() in such a case.
             * We track if fd points to an opened directory in fd_read
             * field of smbwk_dir structure */
            if ((fd = smbc_opendir(buf_string(c->url))) < 0) {
                LOG_ERR("smbc_opendir() returned error. url: %s, go_type: %d\n",
                    buf_string(c->url), type);
                if (errno == ETIMEDOUT)
                    exit(ESTAT_NOCONNECT);
                smbwk_url_suspend(c);
                return -1;
            }
            fd_real = 1;
            break;
        default:
            LOG_ERR("unknown smbwk_go_type %d, url: %s\n",
                type, buf_string(c->url));
            return -1;
    }
   
    if (c->fd_real == 1)
        if (smbc_closedir(c->fd) < 0)
            LOG_ERR("smbc_closedir() returned error. url: %s, go_type: %d\n",
                buf_string(c->url), type);
    
    c->fd = fd;
    c->fd_real = fd_real;
    c->skip_bucks = SKIP_BUCKS_NONE;
    return 1;
}

struct dt_walker smbwk_walker = {
    smbwk_readdir,
    smbwk_go,
};

