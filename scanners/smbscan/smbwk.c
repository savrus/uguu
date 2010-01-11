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

#include "dt.h"
#include "smbwk.h"
#include "log.h"


/* appends to null-terminated url string new file/dir
 * return 0 if name is too long, 1 otherwise */
static int smbwk_url_append(char *url, size_t len, char *name)
{
    size_t n, nn;

    n = strlen(url);
    nn = strlen(name);
    if (n + nn + 2 > len)
        return 0;
    url[n++] = '/';
    strcpy(&url[n], name);
    return 1;
}

/* suspends the last file/dir from nll-terminated url string */
static void smbwk_url_suspend(char *url)
{
    char *c;
    if ((c = strrchr(url, '/')) != NULL)
        *c = 0;
}

/* undo the last suspend of file/dir from nll-terminated url string */
static void smbwk_url_suspend_undo(char *url)
{
    int n = strnlen(url, SMBWK_PATH_MAX_LEN);
    if (n < SMBWK_PATH_MAX_LEN - 1)
        url[n] = '/';
}

/* reallocate null-terminated url string to have length new_len. new_len must exceed strlen(url) */
/* returns 0 if failed, 1 otherwise */
static int smbwk_url_realloc(char **url, size_t new_len)
{
    char *p = (char *) realloc(*url, new_len * sizeof(char));
    if (p == NULL) {
        LOG_ERR("realloc() returned NULL\n");
        return 0;
    }
    *url = p;
    return 1;
}

void smbwk_auth(const char *srv, 
                const char *shr,
                char *wg, int wglen, 
                char *un, int unlen,
                char *pw, int pwlen)
{
}

int smbwk_init(void *curdir)
{
    struct smbwk_dir *c = (struct smbwk_dir*) curdir;
    
    c->ctx = smbc_new_context();
    smbc_option_set(c->ctx, "user", "guest");
    //smbc_setOptionUrlEncodeReaddirEntries(c->ctx, 1);

    if (smbc_init_context(c->ctx) != c->ctx) {
        LOG_ERR("smbc_init_context() failed\n");
        return -1;
    }

    if (smbc_init(smbwk_auth, 0) != 0) {
        LOG_ERR("smbc_init() failed\n");
        return -1;
    }

    if ((c->fd = smbc_opendir(c->url)) < 0) {
        LOG_ERR("smbc_opendir() failed\n");
        c->fd_real = 0;
        return -1;
    }
    c->fd_real = 1;

    return 1;
}

int smbwk_fini(void *curdir)
{
    struct smbwk_dir *c = (struct smbwk_dir*) curdir;
    int ret = 1;    
    
    if (c->fd_real == 1)
        if (smbc_closedir(c->fd) < 0)
            LOG_ERR("smbc_closedir() returned error. url: %s\n", c->url);
    c->fd_real = 0;

    if (smbc_free_context(c->ctx, 1) == 1) {
        LOG_ERR("smbc_free_context() failed\n");
        ret = -1;
    }

    return ret;
}

struct dt_dentry * smbwk_readdir(void *curdir)
{
    struct smbwk_dir *c = (struct smbwk_dir*) curdir;
    struct smbc_dirent *de;
    struct dt_dentry *d;
    struct stat s;
    int skip = 0;
    
    while ((de = smbc_readdir(c->fd)) != NULL) {
        if (!strcmp(de->name,".") || !strcmp(de->name,".."))
           continue;
        if ((de->smbc_type == SMBC_FILE_SHARE)
            || (de->smbc_type == SMBC_DIR)
            || (de->smbc_type == SMBC_FILE))
            break;
    }
    if (de == NULL)
        return NULL;

    d = (struct dt_dentry *) calloc(1, sizeof(struct dt_dentry));
    if (d == NULL) {
        LOG_ERR("calloc() returned NULL\n");
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
            while (smbwk_url_append(c->url, c->url_len, de->name) == 0) {
                if (smbwk_url_realloc(&(c->url), c->url_len + SMBWK_FILENAME_LEN) == 1)
                    c->url_len += SMBWK_FILENAME_LEN;
                else {
                    skip = 1;
                    break;
                }
            }
            if (skip == 0) {
                if (smbc_stat(c->url, &s) == 0)
                    d->size = s.st_size;
                smbwk_url_suspend(c->url);
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
            smbwk_url_suspend(c->url);
            break;
        case DT_GO_SIBLING:
            smbwk_url_suspend(c->url);
            if (smbwk_url_append(c->url, SMBWK_PATH_MAX_LEN, name) == 0){
                LOG_ERR("smbwk_url_append() returned error. url: %s, append: %s, go_type: %d\n",
                        c->url, name, type);
                smbwk_url_suspend_undo(c->url);
                return -1;
            }
            break;
        case DT_GO_CHILD:
            if (smbwk_url_append(c->url, SMBWK_PATH_MAX_LEN, name) == 0){
                LOG_ERR("smbwk_url_append() returned error. url: %s, append: %s, go_type: %d\n",
                        c->url, name, type);
                return -1;
            }
            break;
        default:
            LOG_ERR("unknown smbwk_go_type %d, url: %s\n", type, c->url);
            return -1;
    }
   
    if (type != DT_GO_PARENT) {
        /* 'dir tree' engine won't request readdir afrer go_parent, so we don't
         * have to call smbc_opendir() in such a case. We track if fd points to an
         * opened directory in fd_read field of smbwk_dir structure */
        if ((fd = smbc_opendir(c->url)) < 0) {
            LOG_ERR("smbc_opendir() returned error. url: %s, go_type: %d\n", c->url, type);
            if (type == DT_GO_CHILD)
                smbwk_url_suspend(c->url);
            return -1;
        }
        fd_real = 1;
    } else
        fd_real = 0;

    if (c->fd_real == 1)
        if (smbc_closedir(c->fd) < 0)
            LOG_ERR("smbc_closedir() returned error. url: %s, go_type: %d\n", c->url, type);
    
    c->fd = fd;
    c->fd_real = fd_real;
    return 1;
}

struct dt_walker smbwk_walker = {
    smbwk_init,
    smbwk_fini,
    smbwk_readdir,
    smbwk_go,
};

int smbwk_init_curdir(struct smbwk_dir *c, char *host)
{
    c->url = (char *) malloc((SMBWK_PATH_MAX_LEN + SMBWK_FILENAME_LEN) * sizeof(char));
    if(c->url == NULL) {
        LOG_ERR("malloc() reutrned NULL\n");
        return -1;
    }
    c->url_len = SMBWK_PATH_MAX_LEN + SMBWK_FILENAME_LEN;
    if (strnlen(host, SMBWK_PATH_MAX_LEN) > SMBWK_PATH_MAX_LEN - 10) {
        LOG_ERR("bad argument. host is too long\n");
        return -1;
    }
    sprintf(c->url, "smb://%s", host);
    return 1;
}

int smbwk_fini_curdir(struct smbwk_dir *c)
{
    free(c->url);
    return 1;
}

