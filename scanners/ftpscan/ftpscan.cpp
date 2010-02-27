/* ftpscan - ftp scanner
 *
 * Copyright (c) 2010, Radist <radist.nt@gmail.com>
 * All rights reserved.
 *
 * Read the COPYING file in the root of the source tree.
 */

#ifdef _WIN32

#include <Windows.h>

#define usleep Sleep

#else

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#endif//_WIN32

#include <stdio.h>
#include <string.h>
#include <string>

#ifdef _MSC_VER
#include <unordered_map>
#define strdup _strdup
#else
#include <tr1/unordered_map>
#include "vs_s.h"
#endif

#include "dt.h"
#include "logpp.h"
#include "FtpSockLib.h"
#include "estat.h"

#define DEFAULT_ANSI_CODEPAGE "latin1"
#define LS_R_BUFFER_LEN 8192

class CFtpControlEx
: public CFtpControl
{
public:
	CFtpControlEx() : curpath("/"), do_list(true), errors(10), lscache(false), lstype(NO_DTP) {}
	~CFtpControlEx() {
		if (!do_list) FindClose(findinfo);
		for (std::tr1::unordered_map<std::string, char*>::iterator it = cache.begin(); cache.end()!=it; ++it)
			free((void*)(it->second));
	}
	class LogonError {};
	std::string curpath;
	bool do_list;
	FtpFindInfo findinfo;
	int errors;
	bool lscache;
	DTP_TYPE lstype;
	bool FindFirstFile(const char *DirOrMask, FtpFindInfo &FindInfo);
	bool CacheDirs(bool Active);
private:
	std::tr1::unordered_map<std::string, char*> cache;
	void extend_cache(std::string &_dir, char *data);
};

bool CFtpControlEx::FindFirstFile(const char *DirOrMask, FtpFindInfo &FindInfo)
{
	if (lscache)
	{
		std::tr1::unordered_map<std::string, char*>::iterator it = cache.find(DirOrMask);
		if (cache.end() == it)
		{
			LOG_ERR("Path not cached: %s\n", DirOrMask);
			return false;
		}
		FindInfo.FindPtr = FindInfo.FindBuff = it->second;
		FindInfo.ConvertBuff = NULL;
		cache.erase(it);
		if (FindNextFile(FindInfo))
			return true;
		else
		{
			FindClose(FindInfo);
			return false;
		}
	}
	else
		return CFtpControl::FindFirstFile(DirOrMask, FindInfo, lstype);
}

bool CFtpControlEx::CacheDirs( bool Active )
{
	try {
		if (!ChDir("/") || SendCmd("TYPE A") != '2' || !SendCmdConn("LIST -R /", Active)) return false;
		char buffer[LS_R_BUFFER_LEN+1], *bptr;
		int buflen = 0, dlen = 0, state = 0, n = 0;
		CStrBuf dbuff;
		std::string curdir = "/";
		while (1)
		{
			buflen += RecvData((void *)(buffer + buflen), LS_R_BUFFER_LEN - buflen, true);
			buffer[buflen] = 0;
			bptr = buffer;
			switch (state)
			{
			case 0:
				if (!buflen)
					return false;
				if (char *ptr = strstr(buffer, "\r\n/:\r\n"))
				{
					bptr = ptr + 6;
					buflen -= bptr - buffer;
				}
			case 1:
				if (buflen <= 2)
				{
					dbuff.setsize(dlen + buflen + 1);
					memcpy(dbuff.buf() + dlen, buffer, buflen);
					dbuff.buf()[dlen + buflen] = 0;
					extend_cache(curdir, dbuff.release());
					lscache = cache.size() > 1;
					LOG_ERR("%u dir(s) cached%s\n", (unsigned int)cache.size(), lscache ? "" : ", server doesn't support recursive listing!");
					return true;
				}
			do
			{
				if (char *ptr = strstr(bptr, "\r\n/"))
				{
					n = ptr - bptr;
					state = 2;
				}
				else n = max(buflen - 2, 0);
				dbuff.setsize(dlen + n + 1);
				memcpy(dbuff.buf() + dlen, bptr, n);
				dbuff.buf()[dlen += n] = 0;
				bptr += n;
				buflen -= n;
				if (1 == state)
					break;
				extend_cache(curdir, dbuff.release());
				dlen = 0;
				bptr += 2;
				buflen -= 2;
			case 2:
				if (char *ptr = strstr(bptr, ":\r\n"))
				{
					*ptr = 0;
					n = ptr + 3 - bptr;
					state = 1;
				}
				else n = max(buflen - 2, 0);
				{
					char c = bptr[n];
					bptr[n] = 0;
					curdir.append(bptr);
					bptr[n] = c;
					if (curdir.find("\r\n") != std::string::npos) return false;
				}
				bptr += n;
				buflen -= n;
			}
			while (1 == state);
			}
			memmove_s(buffer, LS_R_BUFFER_LEN, bptr, buflen);
		}
	} catch (const CFtpControl::NetDataError) {
		return false;
	}
}

void CFtpControlEx::extend_cache(std::string &_dir, char *data)
{
	if( ICONV_ERROR != _to_utf8 && _dir.size() > 1 ){
		iconv_cchar *src = const_cast<char *>(_dir.c_str());
		char buff[2*VSPRINTF_BUFFER_SIZE], *dst = buff;
		size_t srclen = _dir.size(), dstlen = 2*VSPRINTF_BUFFER_SIZE;
		if( !iconv(_to_utf8, &src, &srclen, &dst, &dstlen) )
		{
			free((void*)data);
			return;
		}
		*dst = '\0';
		_dir = buff;
	}
	if (cache.find(_dir) == cache.end())
		cache.insert(std::tr1::unordered_map<std::string, char*>::value_type(_dir, data));
	_dir.clear();
}

//////////////////////////////////////////////////////////////////////////
// dt_walker routines

#define SAFE_FTP_CALL(...) \
	while(1) { \
		try { \
			__VA_ARGS__ \
		} catch(const CFtpControl::NetworkError) { \
			if( ftp->errors--<0 ) \
				throw; \
			usleep(1000); \
			TryReconnect(ftp); \
		} \
	}

static void TryReconnect(CFtpControlEx *ftp)
{
	ftp->Quit();
	if( !ftp->tryconn() ) {
		LOG_ERR("Cannot connect %d.%d.%d.%d:%d\n", 
			 ftp->ServerIP        & 0xFF,
			(ftp->ServerIP >>  8) & 0xFF,
			(ftp->ServerIP >> 16) & 0xFF,
			 ftp->ServerIP >> 24,
			 ftp->ServerPORT);
		throw CFtpControl::NetworkError();
	}
	if( !ftp->Logon() ) {
		LOG_ERR("Cannot logon: %s\n", ftp->GetLastResponse());
		throw CFtpControl::NetworkError();
	}
	ftp->ChDir(ftp->curpath.c_str());
}

static struct dt_dentry *fill_dentry(FtpFindInfo &fi)
{
	struct dt_dentry *result = dt_alloc();
	result->type = fi.Data.flagtrycwd?DT_DIR:DT_FILE;
	result->name = strdup(fi.Data.name);
	if( DT_FILE == result->type )
		result->size = fi.Data.size;
	return result;
}

static bool valid_utf8(const char *str)
{
	LOG_ASSERT(str, "Bad arguments\n");
	for (char ch; (ch = *str); ++str) {
		if (0x80 & ch) {
			if (!(0x40 & ch)) return false;
			if ((0xC0 & *++str) != 0x80) return false;
			if (!(0x20 & ch)) continue;
			if ((0xC0 & *++str) != 0x80) return false;
			if (!(0x10 & ch)) continue;
			if ((0xC0 & *++str) != 0x80) return false;
			if ((unsigned char)ch > 0xF4) return false;
		}
	}
	return true;
}

static struct dt_dentry * ftp_readdir_fn(CFtpControlEx *ftp)
{
	do {
		if(ftp->do_list) {
			SAFE_FTP_CALL(
				if( ftp->FindFirstFile(ftp->curpath.c_str(), ftp->findinfo) ) {
					ftp->do_list = false;
					break;
				} else return NULL;
			)
		} else {
			if( !ftp->FindNextFile(ftp->findinfo) ) {
				ftp->FindClose(ftp->findinfo);
				ftp->do_list = true;
				return NULL;
			}
		}
	} while( !strcmp(".", ftp->findinfo.Data.name) || 
			!strcmp("..", ftp->findinfo.Data.name) || 
			!valid_utf8(ftp->findinfo.Data.name));
	return fill_dentry(ftp->findinfo);
}

static int ftp_go_somewhere(dt_go type, char *name, CFtpControlEx *ftp)
{
	ftp->do_list = true;
	std::string olddir = ftp->curpath;
	if (DT_GO_CHILD != type)
		ftp->curpath.resize(ftp->curpath.rfind("/", ftp->curpath.size()-2)+1);
	if (DT_GO_PARENT != type) {
		ftp->curpath += name;
		ftp->curpath += "/";
	}
	if ( ftp->lscache ) return 1;
	SAFE_FTP_CALL(
		if( ftp->ChDir(ftp->curpath.c_str()) ) return 1;
		else {
			LOG_ERR("CWD failed for \"%s\", response is\n\t%s", ftp->curpath.c_str(), ftp->GetLastResponse());
			ftp->curpath = olddir;
			return -1;
		}
	)
}

static struct dt_walker walker = {
    (dt_readdir_fn) &ftp_readdir_fn,
    (dt_go_fn) &ftp_go_somewhere,
};

//////////////////////////////////////////////////////////////////////////
// main
//////////////////////////////////////////////////////////////////////////

#define __TOSTRING(x) #x
#define _TOSTRING(x) __TOSTRING(x)
#ifdef _WIN32
#define _DIR_SLASH '\\'
#else
#define _DIR_SLASH '/'
#endif

static void usage(char *binname, int err)
{
    char *bin = strrchr(binname, _DIR_SLASH);
    if (bin) binname = bin + 1;
	fprintf(stderr, "Usage: %s [-l] [-f] [(-c|-C) cp] [-P##] [-t###] [(-R|-M)#] [-u username] [-p password] [-h] host_ip\n", binname);
    fprintf(stderr, "  -l\tlookup mode (detect if there is anything available)\n");
    fprintf(stderr, "  -f\tprint full paths (debug output)\n");
    fprintf(stderr, "  -c cp\tset codepage for non-utf8 servers (default is " DEFAULT_ANSI_CODEPAGE ")\n");
    fprintf(stderr, "  -C cp\tforce server codepage (without detecting utf8)\n");
    fprintf(stderr, "  -P##\tuse non-default port ## for ftp control connection\n");
    fprintf(stderr, "  -t###\tconnection timeout ### in seconds (default is " _TOSTRING(DEF_TIMEOUT) ")\n");
	fprintf(stderr, "  -R#\tscan with LIST -R command, # = a|p for active or passive mode\n");
	fprintf(stderr, "  -M#\tdisable quick scan, # = a|p for active or passive mode\n");
    fprintf(stderr, "  -h\tprint this help\n");
    exit(err);
}

int main(int argc, char *argv[])
{
#if defined(_WIN32) && defined(_DEBUG)
	_CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_DEBUG );
	_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_DEBUG );
	_CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_DEBUG );
	_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF | _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG));
#endif // _DEBUG

    struct dt_dentry d = {DT_DIR, const_cast<char*>(""), 0, NULL, NULL, NULL, 0};
    CFtpControlEx curdir;
    bool full = false, lookup = false;
    char *host;

    CFtpControl::DefaultAnsiCP = DEFAULT_ANSI_CODEPAGE;

	int i = 0;
	while (++i < argc && argv[i][0] == '-') {
		if (argv[i][1] == '-' && argv[i][2] == 0) {
			i++;
			break;
		}
		switch (argv[i][1]) {
			case 'l':
				lookup = true;
				break;
			case 'f':
				full = true;
				break;
			case 'c':
				CFtpControl::DefaultAnsiCP = argv[++i];
				break;
			case 'C':
				curdir.SetConnCP(argv[++i]);
				break;
			case 'P':
				curdir.ServerPORT = atoi(argv[i]+2);
				break;
			case 't':
				curdir.timeout_sec = atoi(argv[i]+2);
				break;
			case 'u':
				curdir.login = argv[++i];
				break;
			case 'p':
				curdir.pass = argv[++i];//TODO: read from stdin
				break;
			case 'R':
			case 'M':
				if (NO_DTP != curdir.lstype || (argv[i][2] != 'a' && argv[i][2] != 'p'))
					usage(argv[0], ESTAT_FAILURE);
				curdir.lstype = argv[i][2] == 'a' ? DTP_ACTIVE : DTP_PASSIVE;
				curdir.lscache = argv[i][1] == 'R';
				break;
			case 'h':
				usage(argv[0], ESTAT_SUCCESS);
			default:
				usage(argv[0], ESTAT_FAILURE);
		}
	}
	if (i+1 != argc)
		usage(argv[0], ESTAT_FAILURE);
	host = argv[i];

	curdir.ServerIP = inet_addr(host);

	try {
		TryReconnect(&curdir);
		if (curdir.lscache)
		{
			if (curdir.CacheDirs(DTP_ACTIVE == curdir.lstype))
			{
				if (!curdir.lscache)
				{
					if (lookup)
						return ESTAT_FAILURE;
					curdir.lstype = NO_DTP;
				}
			}
			else return ESTAT_FAILURE;
		}
		else if (NO_DTP != curdir.lstype)
			curdir.SendCmd("TYPE A");
		if (lookup) {
			if (curdir.lscache)
				return ESTAT_SUCCESS;
			if (struct dt_dentry *probe = walker.readdir(&curdir))
				return dt_free(probe), ESTAT_SUCCESS;
			else
				return ESTAT_FAILURE;
		}
		if (full)
			dt_full(&walker, &d, &curdir);
		else
			dt_reverse(&walker, &d, &curdir);
	} catch(const CFtpControl::NetDataError) {
		return ESTAT_FAILURE;
	} catch(const CFtpControl::NetworkError) {
		return ESTAT_NOCONNECT;
	} catch(const CFtpControlEx::LogonError) {
		return ESTAT_FAILURE;
	}

    return ESTAT_SUCCESS;
}

