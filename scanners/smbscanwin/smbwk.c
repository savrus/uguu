/* smbwk - smb walker for 'dir tree' engine
 *
 * Copyright 2009, 2010, savrus
 * Copyright 2010, Radist <radist.nt@gmail.com>
 * Read the COPYING file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <Lm.h>
// #include <sys/types.h>
// #include <sys/stat.h>
// #include <unistd.h>
#include <string.h>

#include "dt.h"
#include "estat.h"
#include "smbwk.h"
#include "log.h"
#include "wbuf.h"
#include "stack.h"

#define SHARELIST_TMP_DECLARE size_t listlen = 0, listsize = 0, itemlen
#define SHARELIST_APPEND_WSTR(slist, wstr) \
	do { \
		itemlen = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL); \
		if (!itemlen) \
			break; \
		while (itemlen + 1 > listsize - listlen) \
			slist = (char *)realloc((void *)(slist), listsize += SMBWK_GROW_SHARELIST); \
		itemlen = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, (slist) + listlen, listsize - listlen, NULL, NULL); \
		listlen += itemlen; \
		listlen[slist] = 0; \
	} while (0)
#define SHARELIST_START_ENUM(wkdir) (wkdir).next_share = (wkdir).share_list

#define NETWORK_LASTERROR(err) \
	((ERROR_BAD_NETPATH == (err))||(ERROR_NETWORK_UNREACHABLE == (res)))

static char *smbwk_getshare(struct smbwk_dir *c)
{
	char *res;
	if (!c->next_share || !c->next_share[0])
		return NULL;
	res = _strdup(c->next_share);
	c->next_share += 1 + strlen(c->next_share);
	return res;
}

static int smbwk_fillshares(struct smbwk_dir *c) 
{
	NETRESOURCE netenum, *item;
	HANDLE hEnum;
	DWORD res, rescount, bufsize, i;
	void *buffer;
	SHARELIST_TMP_DECLARE;
	wchar_t *name;
	enum{BUF_SIZE = 16384};
	memset(&netenum, 0, sizeof(NETRESOURCE));
	netenum.dwScope = RESOURCE_CONNECTED;
	netenum.dwType = RESOURCETYPE_DISK;
	netenum.dwDisplayType = RESOURCEDISPLAYTYPE_GENERIC;
	netenum.dwUsage = RESOURCEUSAGE_CONTAINER;
	netenum.lpRemoteName = (LPWSTR)wbuf_string(c->url);
	if ((res = WNetOpenEnum(RESOURCE_GLOBALNET, RESOURCETYPE_DISK, 0, &netenum, &hEnum)) != NO_ERROR) {
		LOG_ERR("WNetOpenEnum failed (%d)\n", res);
		return (ERROR_NO_NET_OR_BAD_PATH == res || ERROR_NO_NETWORK == res) ? ESTAT_NOCONNECT : ESTAT_FAILURE;
	}

	buffer = malloc(BUF_SIZE);
	rescount = (DWORD)(-1);
	bufsize = BUF_SIZE;
	do {
		memset(buffer, 0, BUF_SIZE);
		res = WNetEnumResource(hEnum, &rescount, buffer, &bufsize);
		if (ERROR_MORE_DATA == res)
			res = NO_ERROR;
		else if (NO_ERROR != res) {
			if (ERROR_NO_MORE_ITEMS == res)
				break;
			LOG_ERR("WNetEnumResource failed (%d)\n", res);
			free(buffer);
			WNetCloseEnum(hEnum);
			return (ERROR_NO_NET_OR_BAD_PATH == res || ERROR_NO_NETWORK == res) ? ESTAT_NOCONNECT : ESTAT_FAILURE;
		}
		for (i = 0, item = (LPNETRESOURCE)buffer; i < rescount; i++, item++) {
			name = wcschr(item->lpRemoteName + 2, L'\\');
			if (!name)
				continue;
			++name;
			if (*name)
				SHARELIST_APPEND_WSTR(c->share_list, name);
		}
		
	} while (rescount);
	free(buffer);

	WNetCloseEnum(hEnum);
	return ESTAT_SUCCESS;
}

static int smbwk_fillallshares(struct smbwk_dir *c, int adm) 
{
	DWORD res, resread = 0, rescount = 0, resh = 0, i;
	BYTE *buffer;
	PSHARE_INFO_1 item;
	SHARELIST_TMP_DECLARE;
	enum{BUF_SIZE = 16384};

	do {
		res = NetShareEnum((LPWSTR)wbuf_string(c->url) + 2, 1, &buffer, BUF_SIZE, &resread, &rescount, &resh);
		if (NERR_Success != res && ERROR_MORE_DATA != res) {
			LOG_ERR("NetShareEnum failed (%d)\n", res);
			return NETWORK_LASTERROR(res) ? ESTAT_NOCONNECT : ESTAT_FAILURE;
		}
		for (i = 0, item = (PSHARE_INFO_1)buffer; i < resread; i++, item++) {
			switch (item->shi1_type)
			{
			case STYPE_SPECIAL:
				if (!adm)
					continue;
			case STYPE_DISKTREE:
				break;
			default:
				continue;
			}
			if (item->shi1_netname && *(item->shi1_netname) && 
				(adm || _wcsicmp(item->shi1_netname, L"print$")))
				SHARELIST_APPEND_WSTR(c->share_list, item->shi1_netname);
		}
		NetApiBufferFree((void *)buffer);
	} while (ERROR_MORE_DATA == res);
	return ESTAT_SUCCESS;
}

static struct smbwk_urlpath * smbwk_urlpath_alloc() 
{
    struct smbwk_urlpath *up;

    up = (struct smbwk_urlpath *) malloc(sizeof(struct smbwk_urlpath));
    if (up == NULL)
        LOG_ERRNO("malloc() returned NULL\n");
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
    size_t n;
    wchar_t *wname;

    if ((up = smbwk_urlpath_alloc()) == NULL)
        return 0;
    up->urlpos = wbuf_strlen(c->url);
    stack_push(&c->ancestors, &up->parent);

    n = MultiByteToWideChar(CP_UTF8, 0, name, -1, NULL, 0);
	LOG_ASSERT(n, "MultiByteToWideChar failed with string \"%s\"\n", name);
    __try {
        wname = (wchar_t *)_alloca(n * sizeof(wchar_t));
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        LOG_ERRNO("_alloca() raised stack overflow.\n");
        stack_pop(&c->ancestors);
        return 0;
    }
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, n);
    wbuf_appendf(c->url, L"\\%s", wname);
    if (wbuf_error(c->url))
        return 0;
    return 1;
}

/* suspends the last file/dir from url string */
static void smbwk_url_suspend(struct smbwk_dir *c)
{
    struct smbwk_urlpath *up;

    up = stack_data(stack_pop(&c->ancestors), struct smbwk_urlpath , parent);
    wbuf_chop(c->url, up->urlpos);
    free(up);
}

struct dt_dentry * smbwk_readdir(void *curdir)
{
    struct smbwk_dir *c = (struct smbwk_dir*) curdir;
    struct dt_dentry *d = NULL;
    char *name;
	size_t len;

	if (c->subdir) {
		while (!d && INVALID_HANDLE_VALUE != c->find) {
			if (wcscmp(c->data.cFileName, L".") && wcscmp(c->data.cFileName, L"..") &&
				(len = WideCharToMultiByte(CP_UTF8, 0, c->data.cFileName, -1, NULL, 0, NULL, NULL)) > 0) {
				d = dt_alloc();
				if (d == NULL) 
					return NULL;
				d->type = c->data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY ? DT_DIR : DT_FILE;
				d->name = malloc(len);
				if (d->name == NULL) {
					LOG_ERRNO("malloc() returned NULL\n");
					free(d);
					return NULL;
				}
				WideCharToMultiByte(CP_UTF8, 0, c->data.cFileName, -1, d->name, len, NULL, NULL);
				d->size = c->data.nFileSizeLow + c->data.nFileSizeHigh*((__int64)MAXDWORD+1);
			}
			if (!FindNextFile(c->find, &c->data)) {
				FindClose(c->find);
				c->find = INVALID_HANDLE_VALUE;
				if (ERROR_NO_MORE_FILES != GetLastError())
					LOG_ERR("Enumeration failed for %S (%d)\n", wbuf_string(c->url), GetLastError());
			}
		}
	} else if (name = smbwk_getshare(c)) {
		d = dt_alloc();
		if (d == NULL) 
			return NULL;
		d->type = DT_DIR;
		d->name = name;
		d->size = 0;
	}

    return d;
}

static int smbwk_go(dt_go type, char *name, void *curdir)
{
    struct smbwk_dir *c = (struct smbwk_dir*) curdir;
    int res;
    size_t newpathlen;

    switch (type) {
        case DT_GO_PARENT:
            smbwk_url_suspend(c);
// 			if (!--c->subdir)
// 				SHARELIST_START_ENUM(*c);
            break;
        case DT_GO_CHILD:
            if (smbwk_url_append(c, name) == 0){
                LOG_ERR("smbwk_url_append() returned error. url: %S, append: %s, go_type: %d\n",
                        wbuf_string(c->url), name, type);
                return -1;
            }
			c->subdir++;
            /* 'dir tree' engine won't request readdir afrer go_parent,
            * so we don't have to call FindFirstFile() in such a case. */
            newpathlen = wbuf_strlen(c->url);
            wbuf_append(c->url, L"\\*");
            memset((void*)&c->data, 0, sizeof(c->data));
            c->find = FindFirstFile(wbuf_string(c->url), &c->data);
            wbuf_chop(c->url, newpathlen);
            if (INVALID_HANDLE_VALUE == c->find) {
                LOG_ERR("Enumeration failed for %S (%d)\n", wbuf_string(c->url), res = GetLastError());
                if (NETWORK_LASTERROR(res)) exit(ESTAT_NOCONNECT);
                smbwk_url_suspend(c);
                c->subdir--;
                return -1;
            }
            break;
        default:
            LOG_ERR("unknown smbwk_go_type %d, url: %S\n", type, wbuf_string(c->url));
            return -1;
    }
   
    return 1;
}

struct dt_walker smbwk_walker = {
    smbwk_readdir,
    smbwk_go,
};

int smbwk_open(struct smbwk_dir *c, wchar_t *host, wchar_t *username, wchar_t *password, enum_type enum_hidden_shares, int *wnet_cancel)
{
	NETRESOURCE conn;
	int res;
	if((c->url = wbuf_alloc()) == NULL) {
		return ESTAT_FAILURE;
	}
	wbuf_appendf(c->url, L"\\\\%s", host);
	if (wbuf_error(c->url)) 
		return ESTAT_FAILURE;
	memset(&conn, 0, sizeof(NETRESOURCE));
	conn.dwDisplayType = RESOURCETYPE_DISK;
	conn.lpRemoteName = (LPWSTR)wbuf_string(c->url);
	c->share_list = NULL;
	*wnet_cancel = 1;
	switch (res = WNetAddConnection2(&conn, password, username, 0)) {
	case ERROR_SESSION_CREDENTIAL_CONFLICT://already connected under other username, let's scan using it
		wnet_cancel = 0;
	case NO_ERROR:
		break;
	case ERROR_BAD_NET_NAME:
	case ERROR_NO_NET_OR_BAD_PATH:
	case ERROR_NO_NETWORK:
		LOG_ERR("Network error or %S is down (%d)\n", wbuf_string(c->url), res);
		smbwk_close(c, 0);
		return ESTAT_NOCONNECT;
	default:
		LOG_ERR("Cann\'t establish connection to %S as %S (%d)\n", wbuf_string(c->url), username, res);
		smbwk_close(c, 0);
		return ESTAT_FAILURE;
	}
	if ((res = (
		ENUM_SKIP_DOLLAR == enum_hidden_shares ?
			(int(*)(struct smbwk_dir *, int))smbwk_fillshares :
			smbwk_fillallshares
		)(c, !(int)enum_hidden_shares)) != ESTAT_SUCCESS) {
			smbwk_close(c, *wnet_cancel);
			return res;
	}
	SHARELIST_START_ENUM(*c);
	c->subdir = 0;
	c->find = INVALID_HANDLE_VALUE;
	stack_init(&c->ancestors);
    return ESTAT_SUCCESS;
}

int smbwk_close(struct smbwk_dir *c, int wnet_cancel)
{
	if (wnet_cancel)
		WNetCancelConnection2(wbuf_string(c->url), 0, TRUE);
	free(c->share_list);
    wbuf_free(c->url);
    stack_rfree(&c->ancestors, smbwk_urlpath_free);
    return 1;
}

