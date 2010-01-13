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
#include "smbwk.h"
#include "log.h"

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

static char *smbwk_getshare(struct smbwk_dir *c)
{
	char *res;
	if (!c->next_share || !c->next_share[0])
		return NULL;
	res = _strdup(c->next_share);
	c->next_share += 1 + strlen(c->next_share);
	return res;
}

static void smbwk_fillshares(struct smbwk_dir *c) 
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
	netenum.lpRemoteName = c->url;
	if ((res = WNetOpenEnum(RESOURCE_GLOBALNET, RESOURCETYPE_DISK, 0, &netenum, &hEnum)) != NO_ERROR) {
		LOG_ERR("WNetOpenEnum failed (%d)\n", res);
		return;
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
			if (ERROR_NO_MORE_ITEMS != res)
			LOG_ERR("WNetEnumResource failed (%d)\n", res);
			break;
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
}

static void smbwk_fillallshares(struct smbwk_dir *c, int adm) 
{
	DWORD res, resread, rescount, resh, i;
	BYTE *buffer;
	PSHARE_INFO_1 item;
	SHARELIST_TMP_DECLARE;
	enum{BUF_SIZE = 16384};

	do {
		res = NetShareEnum(c->url + 2, 1, &buffer, BUF_SIZE, &resread, &rescount, &resh);
		if (NERR_Success != res && ERROR_MORE_DATA != res) {
			LOG_ERR("NetShareEnum failed (%d)\n", res);
			break;
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
			if (item->shi1_netname && *(item->shi1_netname))
				SHARELIST_APPEND_WSTR(c->share_list, item->shi1_netname);
		}
		NetApiBufferFree((void *)buffer);
	} while (ERROR_MORE_DATA == res);
}

/* appends to null-terminated url string new file/dir
 * return 0 if name is too long, 1 otherwise */
static int smbwk_url_append(wchar_t *url, size_t len, char *name)
{
    size_t n, nn;

    n = wcslen(url);
    nn = MultiByteToWideChar(CP_UTF8, 0, name, -1, NULL, 0);
	LOG_ASSERT(nn, "MultiByteToWideChar failed with string \"%s\"\n", name);
    if (n + nn + 1 > len)
        return 0;
    url[n++] = L'\\';
	MultiByteToWideChar(CP_UTF8, 0, name, -1, &url[n], nn);
    return 1;
}

/* suspends the last file/dir from nll-terminated url string */
static void smbwk_url_suspend(wchar_t *url)
{
    wchar_t *c;
    if ((c = wcsrchr(url, L'\\')) != NULL)
        *c = 0;
}

/* undo the last suspend of file/dir from nll-terminated url string */
static void smbwk_url_suspend_undo(wchar_t *url)
{
    int n = wcsnlen(url, SMBWK_PATH_MAX_LEN);
    if (n < SMBWK_PATH_MAX_LEN - 1)
        url[n] = L'\\';
}

/* reallocate null-terminated url string to have length new_len. new_len must exceed strlen(url) */
/* returns 0 if failed, 1 otherwise */
static int smbwk_url_realloc(wchar_t **url, size_t new_len)
{
    wchar_t *p = (wchar_t *) realloc(*url, new_len * sizeof(wchar_t));
    if (p == NULL) {
        LOG_ERR("realloc() returned NULL\n");
        return 0;
    }
    *url = p;
    return 1;
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
				d = (struct dt_dentry *)calloc(1, sizeof(struct dt_dentry));
				if (d == NULL) {
					LOG_ERR("calloc() returned NULL\n");
					return NULL;
				}
				d->type = c->data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY ? DT_DIR : DT_FILE;
				d->name = malloc(len);
				if (d->name == NULL) {
					free(d);
					LOG_ERR("malloc() returned NULL\n");
					return NULL;
				}
				WideCharToMultiByte(CP_UTF8, 0, c->data.cFileName, -1, d->name, len, NULL, NULL);
				d->size = c->data.nFileSizeLow + c->data.nFileSizeHigh*((__int64)MAXDWORD+1);
			}
			if (!FindNextFile(c->find, &c->data)) {
				FindClose(c->find);
				c->find = INVALID_HANDLE_VALUE;
			}
		}
	} else if (name = smbwk_getshare(c)) {
		d = (struct dt_dentry *)calloc(1, sizeof(struct dt_dentry));
		if (d == NULL) {
			LOG_ERR("calloc() returned NULL\n");
			return NULL;
		}
		d->type = DT_DIR;
		d->name = name;
		d->size = 0;
	}

    return d;
}

static int smbwk_go(dt_go type, char *name, void *curdir)
{
    struct smbwk_dir *c = (struct smbwk_dir*) curdir;


    switch (type) {
        case DT_GO_PARENT:
            smbwk_url_suspend(c->url);
			if (!--c->subdir)
				SHARELIST_START_ENUM(*c);
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
			c->subdir++;
            break;
        default:
            LOG_ERR("unknown smbwk_go_type %d, url: %s\n", type, c->url);
            return -1;
    }
   
    if (type != DT_GO_PARENT) {
        /* 'dir tree' engine won't request readdir afrer go_parent, so we don't
        * have to call smbc_opendir() in such a case. We track if fd points to an
        * opened directory in fd_read field of smbwk_dir structure */
		wcscat_s(c->url, c->url_len, L"\\*");//no need another checks or convertation from utf8
		c->find = FindFirstFile(c->url, &c->data);
		smbwk_url_suspend(c->url);
		if (INVALID_HANDLE_VALUE == c->find)
		{
			if (type == DT_GO_CHILD) {
				smbwk_url_suspend(c->url);
				c->subdir--;
			}
			return -1;
		}
    }

    return 1;
}

struct dt_walker smbwk_walker = {
    smbwk_readdir,
    smbwk_go,
};

int smbwk_open(struct smbwk_dir *c, wchar_t *host, wchar_t *username, wchar_t *password, enum_type enum_hidden_shares)
{
	NETRESOURCE conn;
	c->url = (wchar_t *)malloc((SMBWK_PATH_MAX_LEN + SMBWK_FILENAME_LEN) * sizeof(wchar_t));
	if(c->url == NULL) {
		LOG_ERR("malloc() returned NULL\n");
		return -1;
	}
	c->url_len = SMBWK_PATH_MAX_LEN + SMBWK_FILENAME_LEN;
	if (wcsnlen(host, SMBWK_PATH_MAX_LEN) > SMBWK_PATH_MAX_LEN - 10) {
		LOG_ERR("bad argument. host is too long\n");
		free(c->url);
		return -1;
	}
	wcscpy_s(c->url, SMBWK_PATH_MAX_LEN + SMBWK_FILENAME_LEN, L"\\\\");
	wcscat_s(c->url, SMBWK_PATH_MAX_LEN + SMBWK_FILENAME_LEN, host);
	memset(&conn, 0, sizeof(NETRESOURCE));
	conn.dwDisplayType = RESOURCETYPE_DISK;
	conn.lpRemoteName = c->url;
	switch (WNetAddConnection2(&conn, password, username, 0)) {
	case NO_ERROR:
	case ERROR_SESSION_CREDENTIAL_CONFLICT://already connected under other username, let's scan using it
		break;
	default:
		LOG_ERR("Cann\'t establish connection to %S as %S\n", c->url, username);
		smbwk_close(c);
		return -1;
	}
	c->share_list = NULL;
	if (ENUM_SKIP_DOLLAR == enum_hidden_shares)
		smbwk_fillshares(c);
	else
		smbwk_fillallshares(c, !(int)enum_hidden_shares);
	SHARELIST_START_ENUM(*c);
	c->subdir = 0;
	c->find = INVALID_HANDLE_VALUE;
    return 1;
}

int smbwk_close(struct smbwk_dir *c)
{
	WNetCancelConnection2(c->url, 0, TRUE);
	if (c->share_list)
		free(c->share_list);
    free(c->url);
    return 1;
}

