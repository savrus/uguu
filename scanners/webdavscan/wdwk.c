/* wdwk - webdav walker for 'dir tree' engine
 *
 * Copyright 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <neon/ne_session.h>
#include <neon/ne_basic.h>
#include <neon/ne_request.h>
#include <neon/ne_props.h>
#include <neon/ne_xml.h>
#include <neon/ne_alloc.h>
#include <neon/ne_uri.h>

#include "dt.h"
#include "estat.h"
#include "wdwk.h"
#include "log.h"
#include "stack.h"
#include "buf.h"

static struct wdwk_urlpath * wdwk_urlpath_alloc()
{
    struct wdwk_urlpath *up;
    up = (struct wdwk_urlpath *) malloc(sizeof(struct wdwk_urlpath));
    if (up == NULL)
        LOG_ERRNO("malloc() returned NULL\n");
    return up;
}

static void wdwk_urlpath_free(struct stack *s)
{
    struct wdwk_urlpath *up;
    up = stack_data(s, struct wdwk_urlpath, parent);
    free(up);
}

/* appends new file/dir to url
 * return 0 if failed, 1 otherwise */
static int wdwk_url_append(struct wdwk_dir *c, char *name)
{
    struct wdwk_urlpath *up;
    char *escaped;

    if ((up = wdwk_urlpath_alloc()) == NULL)
        return 0;
    up->urlpos = buf_strlen(c->url);
    stack_push(&c->ancestors, &up->parent);

    escaped = ne_path_escape(name);
    buf_appendf(c->url, "%s/", escaped);
    ne_free(escaped);

    if (buf_error(c->url))
        return 0;
    return 1;
}

/* suspends the last file/dir from url string */
static void wdwk_url_suspend(struct wdwk_dir *c)
{
    struct wdwk_urlpath *up;

    up = stack_data(stack_pop(&c->ancestors), struct wdwk_urlpath, parent);
    buf_chop(c->url, up->urlpos);
    free(up);
}

static void * wdwk_res_alloc(void *userdata, const ne_uri *uri)
{
    return ne_calloc(sizeof(struct wdwk_res));
}

static void wdwk_res_free(struct stack *s)
{
    struct wdwk_res *r;
    r = stack_data(s, struct wdwk_res, stack_res);
    ne_free(r->name);
    ne_free(r);
}

static const ne_propname wdwk_props[] = {
    { "DAV:", "getcontentlength" },
    { "DAV:", "resourcetype"},
    { NULL },
};

enum {
    WDWK_RES = NE_PROPS_STATE_TOP + 1,
    WDWK_DIR,
};

static const struct ne_xml_idmap wdwk_idmap[] = {
    { "DAV:", "resourcetype", WDWK_RES },
    { "DAV:", "collection", WDWK_DIR },
};

static int wdwk_startelm(void *userdata, int parent, const char *nspace, const char *name, const char **atts)
{
    ne_propfind_handler *h = (ne_propfind_handler *) userdata;
    struct wdwk_res *r = ne_propfind_current_private(h);
    int id = ne_xml_mapid(wdwk_idmap, NE_XML_MAPLEN(wdwk_idmap), nspace, name);

    if (r == NULL ||
        !((parent == NE_207_STATE_PROP && id == WDWK_RES) ||
          (parent == WDWK_RES && id == WDWK_DIR)))
        return NE_XML_DECLINE;

    if (id == WDWK_DIR)
        r->type = DT_DIR;
    else
        r->type = DT_FILE;
    
    return id;
}

/* workaround if a server forgets to urlencode some characters in names. */
static int wdwk_path_compare(const char *e1, const char *e2)
{
    char *u1, *u2;
    int ret;

    u1 = ne_path_unescape(e1);
    u2 = ne_path_unescape(e2);
    ret = ne_path_compare(u1, u2);
    ne_free(u1);
    ne_free(u2);
    return ret;
}

static void wdwk_result(void *userdata, const ne_uri *uri, const ne_prop_result_set *results)
{
    struct wdwk_dir *c = (struct wdwk_dir *) userdata;
    struct wdwk_res *r = (struct wdwk_res *) ne_propset_private(results);
    const char *size;
    char *p, *name;

    if (!wdwk_path_compare(buf_string(c->url), uri->path)) {
        ne_free(r);
        return;
    }

    size = ne_propset_value(results, &wdwk_props[0]);

    if (size == NULL) {
        LOG_ERR("size element is NULL for %s\n", uri->path);
        size = "0";
    }

    p = ne_strdup(uri->path);
    if (ne_path_has_trailing_slash(p))
        p[strlen(p) - 1] = '\0';
    name = strrchr(p, '/');
    if (name != NULL)
        name++;
    else
        name = p;
    r->name = ne_path_unescape(name);
    ne_free(p);


    if (r->type == DT_FILE) {
        r->size = strtoull(size, &p, 10);
        if (*p)
            r->size = 0;
    }

    stack_push(&c->resources, &r->stack_res);
}

/* returns NE_* */
static int wdwk_fetch(struct wdwk_dir *c)
{
    ne_propfind_handler *h = ne_propfind_create(c->sess, buf_string(c->url), 1);
    int ret;

    stack_rfree(&c->resources, wdwk_res_free);

    ne_xml_push_handler(ne_propfind_get_parser(h),
        wdwk_startelm, NULL, NULL, h);
    ne_propfind_set_private(h, wdwk_res_alloc, NULL, NULL);
    ret = ne_propfind_named(h, wdwk_props, wdwk_result, c);
    ne_propfind_destroy(h);

    return ret;
}


int wdwk_open(struct wdwk_dir *c, char *host, int port)
{
    unsigned int caps;
    int ret;

    c->host = strdup(host);
    if (port == 0)
        c->port = 80;
    else
        c->port = port;

    c->sess = ne_session_create("http", c->host, c->port);
    if (c->sess == NULL) {
        LOG_ERR("ne_session_create returned NULL\n");
        return -1;
    }

    ne_set_useragent(c->sess, "uguu webdavscan");

    ret = ne_options2(c->sess, "/", &caps);

    if (ret != NE_OK) {
        LOG_ERR("Error while connecting: %s\n", ne_get_error(c->sess));
        ne_session_destroy(c->sess);
        free(c->host);
        return -1;
    }

    if (!(caps & (NE_CAP_DAV_CLASS1|NE_CAP_DAV_CLASS2|NE_CAP_DAV_CLASS3))) {
        LOG_ERR("Not a WebDAV class 1-3 capable resource\n");
        ne_session_destroy(c->sess);
        free(c->host);
        return -1;
    }

    if ((c->url = buf_alloc()) == NULL)
        return -1;

    buf_append(c->url, "/");

    if (buf_error(c->url))
        return -1;

    stack_init(&c->resources);
    stack_init(&c->ancestors);

    if (wdwk_fetch(c) != NE_OK) {
        LOG_ERR("Fetch failed: %s\n", ne_get_error(c->sess));
        buf_free(c->url);
        free(c->host);
        ne_session_destroy(c->sess);
        stack_rfree(&c->resources, wdwk_res_free);
        return -1;
    }

    return 1;
}

int wdwk_close(struct wdwk_dir *c)
{
    stack_rfree(&c->ancestors, wdwk_urlpath_free);
    stack_rfree(&c->resources, wdwk_res_free);
    ne_session_destroy(c->sess);
    free(c->host);
    return 0;
}
            
struct dt_dentry * wdwk_readdir(void *curdir)
{
    struct wdwk_dir *c = (struct wdwk_dir *) curdir;
    struct wdwk_res *r;
    struct dt_dentry *d;

    if (c->resources == NULL)
        return NULL;
    
    r = stack_data(stack_pop(&c->resources), struct wdwk_res, stack_res);

    if ((d = dt_alloc()) == NULL) {
        ne_free(r->name);
        ne_free(r);
        return NULL;
    }
    d->size = r->size;
    d->name = strdup(r->name);
    d->type = r->type;
    ne_free(r->name);
    ne_free(r);

    return d;
}

static int wdwk_go(dt_go type, char *name, void *curdir)
{
    struct wdwk_dir *c = (struct wdwk_dir*) curdir;
    int ret;

    switch (type) {
        case DT_GO_PARENT:
            wdwk_url_suspend(c);
            break;
        case DT_GO_CHILD:
            if (wdwk_url_append(c, name) == 0){
                LOG_ERR("wdwk_url_append() returned error. url: %s, append: %s, go_type: %d\n",
                        buf_string(c->url), name, type);
                return -1;
            }

            /* 'dir tree' engine won't request readdir afrer go_parent,
             * so we don't have to call wdc_opendir() in such a case.
             * We track if fd points to an opened directory in fd_read
             * field of wdwk_dir structure */
            if ((ret = wdwk_fetch(c)) != NE_OK) {
                LOG_ERR("wdwk_fetch() returned error. url: %s, go_type: %d\n",
                    buf_string(c->url), type);
                if (ret == NE_TIMEOUT)
                    exit(ESTAT_NOCONNECT);
                wdwk_url_suspend(c);
                return -1;
            }
            break;
        default:
            LOG_ERR("unknown wdwk_go_type %d, url: %s\n",
                type, buf_string(c->url));
            return -1;
    }
   
    return 1;
}

struct dt_walker wdwk_walker = {
    wdwk_readdir,
    wdwk_go,
};

