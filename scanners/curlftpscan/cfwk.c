/* cfwk - curl ftp walker for 'dir tree' engine
 *
 * Copyright 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

/* strndup support */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include <iconv.h>
#include <curl/curl.h>

#include "dt.h"
#include "estat.h"
#include "cfwk.h"
#include "log.h"
#include "stack.h"
#include "buf.h"
#include "ftpparse.h"

#ifdef _MSC_VER
#define strdup _strdup
#endif

static int cfwk_iconv_close(iconv_t cd)
{
    if (cd != (iconv_t) -1)
        return iconv_close(cd);
    return 0;
}

static char* cfwk_iconv(struct cfwk_dir *c, iconv_t cd, char *s, size_t len)
{
    char *ibuf = s;
    char *obuf = c->conv_buffer;
    size_t isize = len;
    size_t osize = c->conv_buffer_size;
    size_t ret;
    char *newstr;

    ret = iconv(cd, &ibuf, &isize, &obuf, &osize);

    /* FIXME: enlarge buffer, report other errors */

    if (ret == -1)
        newstr = strndup(s,len);
    else
        newstr = strndup(c->conv_buffer, obuf - c->conv_buffer);
    if (newstr == NULL)
        LOG_ERR("strndup() returned NULL");
    return newstr;
}


static struct cfwk_urlpath * cfwk_urlpath_alloc()
{
    struct cfwk_urlpath *up;
    up = (struct cfwk_urlpath *) malloc(sizeof(struct cfwk_urlpath));
    if (up == NULL)
        LOG_ERRNO("malloc() returned NULL\n");
    return up;
}

static void cfwk_urlpath_free(struct stack *s)
{
    struct cfwk_urlpath *up;
    up = stack_data(s, struct cfwk_urlpath, parent);
    free(up);
}

/* appends new file/dir to url
 * return 0 if failed, 1 otherwise */
static int cfwk_url_append(struct cfwk_dir *c, char *name)
{
    struct cfwk_urlpath *up;

    if ((up = cfwk_urlpath_alloc()) == NULL)
        return 0;
    up->urlpos = buf_strlen(c->url);
    stack_push(&c->ancestors, &up->parent);

    buf_appendf(c->url, "%s/", name);

    if (buf_error(c->url))
        return 0;
    return 1;
}

/* suspends the last file/dir from url string */
static void cfwk_url_suspend(struct cfwk_dir *c)
{
    struct cfwk_urlpath *up;

    up = stack_data(stack_pop(&c->ancestors), struct cfwk_urlpath, parent);
    buf_chop(c->url, up->urlpos);
    free(up);
}


static struct cfwk_res * cfwk_res_alloc()
{
    struct cfwk_res *res;
    if ((res = (struct cfwk_res *) calloc(1, sizeof(struct cfwk_res))) == NULL)
        LOG_ERR("calloc() returned NULL\n");
    return res;
}

static void cfwk_res_free(struct stack *s)
{
    struct cfwk_res *r;
    r = stack_data(s, struct cfwk_res, stack_res);
    free(r->name);
    free(r);
}

static size_t cfwk_curl_write(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct cfwk_dir *c = (struct cfwk_dir *) userdata;
    char *s = (char *) ptr;
    unsigned int i;
    unsigned int begin = 0;
    int err;
    struct cfwk_res *res = NULL;
    char *str;
    
    for (i = 0; i < size * nmemb; i++) {
        if (s[i] == '\n' || s[i] == 0) {
            buf_appendn(c->tmpres, &s[begin], i - begin);
            /* FIXME: handle on curl return.
             * c->tmpres need to be reinitialized */
            if (buf_error(c->tmpres))
                return begin;

            if (buf_strlen(c->tmpres) == 0) {
                begin = i+1;
                continue;
            }

            if ((str = strdup(buf_string(c->tmpres))) == NULL) {
                LOG_ERR("strdup() returned NULL\n");
                buf_clear(c->tmpres);
                continue;
            }

            err = ftpparse(&c->ftpparse, str, strlen(str));

            res = NULL;

            if (err != 1) {
                LOG_ERR("ftpparse() can't pase string (url %s):\n%s\n",
                        buf_string(c->url), buf_string(c->tmpres));
            } else {
                if (!((c->ftpparse.namelen == 1 && c->ftpparse.name[0] == '.')
                      || (c->ftpparse.namelen == 2 && c->ftpparse.name[0] == '.'
                          && c->ftpparse.name[1] == '.'))) {
                    if ((res = cfwk_res_alloc()) != NULL) {
                        res->name = cfwk_iconv(c, c->conv_from_server, c->ftpparse.name, c->ftpparse.namelen);
                        res->type = c->ftpparse.flagtrycwd && !c->ftpparse.flagtryretr ? DT_DIR : DT_FILE;
                        res->size = !c->ftpparse.flagtrycwd ? c->ftpparse.size : 0;
                    }
                }
            }

            if ((res != NULL) && (res->name != NULL))
                stack_push(&c->resources, &res->stack_res);

            free(str);
            buf_clear(c->tmpres);
            begin = i+1;
        }
    }

    if (begin < i) {
        if (buf_appendn(c->tmpres, &s[begin], i - begin) == 0)
            return begin;
    }
    
    return size * nmemb;
}

static CURLcode cfwk_fetch(struct cfwk_dir *c)
{
    CURLcode rcode;

    rcode = curl_easy_setopt(c->curl, CURLOPT_URL, buf_string(c->url));
    if (rcode != CURLE_OK) {
        LOG_ERR("curl_easy_setopt(CURLOPT_URL): %s\n",
                curl_easy_strerror(rcode));
        return rcode;
    }
    
    if (c->port) {
        /* FIXME: setting this option enables active mode */
        rcode = curl_easy_setopt(c->curl, CURLOPT_FTPPORT, c->port);
        if (rcode != CURLE_OK) {
            LOG_ERR("curl_easy_setopt(CURLOPT_FTPPORT): %s\n",
                    curl_easy_strerror(rcode));
            return rcode;
        }
    }

    rcode = curl_easy_setopt(c->curl, CURLOPT_WRITEFUNCTION, cfwk_curl_write);
    if (rcode != CURLE_OK) {
        LOG_ERR("curl_easy_setopt(CURLOPT_WRITEFUNCTION): %s\n",
                curl_easy_strerror(rcode));
        return rcode;
    }

    rcode = curl_easy_setopt(c->curl, CURLOPT_WRITEDATA, c);
    if (rcode != CURLE_OK) {
        LOG_ERR("curl_easy_setopt(CURLOPT_WRITEDATA: %s\n",
                curl_easy_strerror(rcode));
        return rcode;
    }

    rcode = curl_easy_perform(c->curl);
    if (rcode != CURLE_OK) {
        LOG_ERR("curl_easy_perform(): %s\n",
                curl_easy_strerror(rcode));
        return rcode;
    }

    curl_easy_reset(c->curl);

    return CURLE_OK;
}

int cfwk_open(struct cfwk_dir *c, const char *host, const char *port, const char *cp)
{
    if (port != NULL) {
        if ((c->port = strdup(port)) == NULL) {
            LOG_ERR("strdup() returned NULL\n");
            return -1;
        }
    } else
        c->port = NULL;

    if (cp != NULL) {
        if ((c->conv_from_server = iconv_open("UTF-8", cp)) == (iconv_t) -1) {
            LOG_ERRNO("iconv_open:");
            free(c->port);
            return -1;
        }
        if ((c->conv_to_server = iconv_open(cp, "UTF-8")) == (iconv_t) -1) {
            LOG_ERRNO("iconv_open:");
            cfwk_iconv_close(c->conv_from_server);
            free(c->port);
            return -1;
        }
    } else {
        c->conv_from_server = (iconv_t) -1;
        c->conv_to_server = (iconv_t) -1;
    }

    c->conv_buffer_size = 1024;
    if ((c->conv_buffer = malloc(1024 * sizeof(char))) == NULL) {
        LOG_ERR("malloc() returned NULL\n");
        free(c->port);
    }


    if ((c->curl = curl_easy_init()) == NULL) {
        LOG_ERR("curl_easy_init() returned NULL\n");
        free(c->port);
        free(c->conv_buffer);
        return -1;
    }

    if ((c->url = buf_alloc()) == NULL) {
        free(c->conv_buffer);
        free(c->port);
        return -1;
    }

    if (buf_appendf(c->url, "ftp://%s/", host) == 0) {
        free(c->port);
        free(c->conv_buffer);
        buf_free(c->url);
        return -1;
    }
    
    if ((c->tmpres = buf_alloc()) == NULL) {
        free(c->port);
        free(c->conv_buffer);
        buf_free(c->url);
        return -1;
    }
    
    stack_init(&c->resources);
    stack_init(&c->ancestors);

    if (cfwk_fetch(c) != CURLE_OK) {
        free(c->port);
        free(c->conv_buffer);
        stack_rfree(&c->resources, cfwk_res_free);
        buf_free(c->tmpres);
        buf_free(c->url);
        return -1;
    }
    
    return 1;
}

int cfwk_close(struct cfwk_dir *c)
{
    stack_rfree(&c->ancestors, cfwk_urlpath_free);
    stack_rfree(&c->resources, cfwk_res_free);
    buf_free(c->url);
    buf_free(c->tmpres);
    curl_easy_cleanup(c->curl);
    free(c->port);
    free(c->conv_buffer);
    cfwk_iconv_close(c->conv_from_server);
    cfwk_iconv_close(c->conv_to_server);
    return 0;
}
            
struct dt_dentry * cfwk_readdir(void *curdir)
{

    struct cfwk_dir *c = (struct cfwk_dir *) curdir;
    struct cfwk_res *r;
    struct dt_dentry *d;

    if (c->resources == NULL)
        return NULL;
    
    r = stack_data(stack_pop(&c->resources), struct cfwk_res, stack_res);

    if ((d = dt_alloc()) == NULL) {
        free(r->name);
        free(r);
        return NULL;
    }
    d->size = r->size;
    d->name = r->name;
    d->type = r->type;
    free(r);

    return d;
}

static int cfwk_go(dt_go type, char *name, void *curdir)
{
    struct cfwk_dir *c = (struct cfwk_dir*) curdir;
    char *cname;
    int ret;

    switch (type) {
        case DT_GO_PARENT:
            cfwk_url_suspend(c);
            break;
        case DT_GO_CHILD:
            cname = cfwk_iconv(c, c->conv_to_server, name, strlen(name));
            if (cname == NULL)
                return -1;
            if (cfwk_url_append(c, cname) == 0){
                LOG_ERR("cfwk_url_append() returned error. url: %s, append: %s, go_type: %d\n",
                        buf_string(c->url), cname, type);
                free(cname);
                return -1;
            }
            free(cname);

            /* 'dir tree' engine won't request readdir afrer go_parent,
             * so we don't have to call wdc_opendir() in such a case.
             * We track if fd points to an opened directory in fd_read
             * field of cfwk_dir structure */
            if ((ret = cfwk_fetch(c)) != CURLE_OK) {
                LOG_ERR("cfwk_fetch() returned error. url: %s, go_type: %d\n",
                    buf_string(c->url), type);
                if (ret == CURLE_OPERATION_TIMEDOUT)
                    exit(ESTAT_NOCONNECT);
                cfwk_url_suspend(c);
                return -1;
            }
            break;
        default:
            LOG_ERR("unknown cfwk_go_type %d, url: %s\n",
                type, buf_string(c->url));
            return -1;
    }
    return 1;
}

struct dt_walker cfwk_walker = {
    cfwk_readdir,
    cfwk_go,
};

