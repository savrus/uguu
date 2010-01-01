/* smbwk - smb walker for 'dir tree' engine
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

/* reallocate null-terminated url string to have size new_len. new_len must exceed strlen(url) */
/* returns 0 if failed, 1 otherwise */
static int smbwk_url_realloc(char **url, size_t new_len)
{
    char *p = (char *) realloc(*url, new_len);
    if (p != NULL) {
        *url = p;
        return 1;
    }
    /* FIXME: is malloc() relevant here? realloc() has already failed */
    p = (char *) malloc(new_len);
    if (p == NULL) {
        LOG_ERR("realloc and malloc both returned NULL\n");
        return 0;
    }
    strcpy(p, *url);
    free(*url);
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
        return -1;
    }

    return 1;
}

int smbwk_fini(void *curdir)
{
    struct smbwk_dir *c = (struct smbwk_dir*) curdir;
    int ret = 1;    
    
    /* do not check for errors because we don't really know if c->fd points to an opened dir */
    smbc_closedir(c->fd);
    
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

typedef enum {
    SMBWK_GO_PARENT = 0,
    SMBWK_GO_SIBLING,
    SMBWK_GO_CHILD,
} smbwk_go_type;

static int smbwk_go(char *name, void *curdir, smbwk_go_type type)
{
    struct smbwk_dir *c = (struct smbwk_dir*) curdir;
    int fd;

    switch (type) {
        case SMBWK_GO_PARENT:
            smbwk_url_suspend(c->url);
            break;
        case SMBWK_GO_SIBLING:
            smbwk_url_suspend(c->url);
            if (smbwk_url_append(c->url, SMBWK_PATH_MAX_LEN, name) == 0){
                LOG_ERR("smbwk_url_append() returned error. url: %s, append: %s, go_type: %d\n",
                        c->url, name, type);
                smbwk_url_suspend_undo(c->url);
                return -1;
            }
            break;
        case SMBWK_GO_CHILD:
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
   
    if (type != SMBWK_GO_PARENT) {
        /* 'dir tree' engine won't request readdir afrer go_parent, so we don't
         * have to call smbc_opendir() in such a case. This will cause redundant
         * smbc_closedir() calls but we bear with it */
        if ((fd = smbc_opendir(c->url)) < 0) {
            LOG_ERR("smbc_opendir() returned error. url: %s, go_type: %d\n", c->url, type);
            if (type == SMBWK_GO_CHILD)
                smbwk_url_suspend(c->url);
            return -1;
        }
    }

    /* do not check for errors because we don't really know if c->fd points to an opened dir */
    smbc_closedir(c->fd);
    
    c->fd = fd;
    return 1;
}

int smbwk_goparent(void *curdir)
{
    return smbwk_go(NULL, curdir, SMBWK_GO_PARENT);
}

int smbwk_gosibling(char *name, void *curdir)
{
    return smbwk_go(name, curdir, SMBWK_GO_SIBLING);
}

int smbwk_gochild(char *name, void *curdir)
{
    return smbwk_go(name, curdir, SMBWK_GO_CHILD);
}

struct dt_walker smbwk_walker = {
    smbwk_init,
    smbwk_fini,
    smbwk_readdir,
    smbwk_goparent,
    smbwk_gosibling,
    smbwk_gochild,
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

