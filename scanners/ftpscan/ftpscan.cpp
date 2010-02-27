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
#ifdef _MSC_VER
#define strdup _strdup
#endif

#else

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#endif//_WIN32

#include <stdio.h>
#include <string.h>
#include <string>

#include "dt.h"
#include "logpp.h"
#include "FtpSockLib.h"
#include "estat.h"

#define DEFAULT_ANSI_CODEPAGE "latin1"

//////////////////////////////////////////////////////////////////////////
// dt_walker routines

class CFtpControlEx
: public CFtpControl
{
public:
	CFtpControlEx() : curpath("/"), do_list(true), errors(10) {}
	~CFtpControlEx() {
		if (!do_list) FindClose(findinfo);
	}
	class LogonError {};
	std::string curpath;
	bool do_list;
	FtpFindInfo findinfo;
	int errors;
};

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
    fprintf(stderr, "Usage: %s [-l] [-f] [(-c|-C) cp] [-P##] [-t###] [-u username] [-p password] [-h] host_ip\n", binname);
    fprintf(stderr, "  -l\tlookup mode (detect if there is anything available)\n");
    fprintf(stderr, "  -f\tprint full paths (debug output)\n");
    fprintf(stderr, "  -c cp\tset codepage for non-utf8 servers (default is " DEFAULT_ANSI_CODEPAGE ")\n");
    fprintf(stderr, "  -C cp\tforce server codepage (without detecting utf8)\n");
    fprintf(stderr, "  -P##\tuse non-default port ## for ftp control connection\n");
    fprintf(stderr, "  -t###\tconnection timeout ### in seconds (default is " _TOSTRING(DEF_TIMEOUT) ")\n");
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
		if (lookup) {
			if (struct dt_dentry *probe = walker.readdir(&curdir))
				return dt_free(probe), ESTAT_SUCCESS;
			else
				return ESTAT_FAILURE;
		}
		if (full)
			dt_full(&walker, &d, &curdir);
		else
			dt_reverse(&walker, &d, &curdir);
	} catch(const CFtpControl::NetworkError&) {
		return ESTAT_NOCONNECT;
	} catch(const CFtpControlEx::LogonError&) {
		return ESTAT_FAILURE;
	}

    return ESTAT_SUCCESS;
}

