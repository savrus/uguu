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

extern "C" {
#include "dt.h"
}
#include "logpp.h"
#include "FtpSockLib.h"

//////////////////////////////////////////////////////////////////////////
// dt_walker routines

class CFtpControlEx
: public CFtpControl
{
public:
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
				throw CFtpControl::NetworkError(); \
			usleep(1000); \
			TryReconnect(ftp); \
		} \
	}

void TryReconnect(CFtpControlEx *ftp)
{
	ftp->Quit();
	if( !ftp->tryconn() )
		throw CFtpControl::NetworkError();
	if( !ftp->Logon() ) {
		LOG_ERR("Cannot logon: %s\n", ftp->GetLastResponse());
		throw CFtpControl::NetworkError();
	}
	ftp->ChDir(ftp->curpath.c_str());
}

int ftp_init_fn(CFtpControlEx *ftp)
{
	ftp->curpath = "/";
	ftp->do_list = true;
	ftp->errors = 10;
	TryReconnect(ftp);
	return 1;
}

int ftp_fini_fn(CFtpControlEx *ftp)
{
	ftp->Quit();
	return 1;
}

struct dt_dentry *fill_dentry(FtpFindInfo &fi)
{
	struct dt_dentry *result = (struct dt_dentry*)calloc(1, sizeof(struct dt_dentry));
	result->type = fi.Data.flagtrycwd?DT_DIR:DT_FILE;
	result->name = strdup(fi.Data.name);
	if( DT_FILE == result->type )
		result->size = fi.Data.size;
	return result;
}


struct dt_dentry * ftp_readdir_fn(CFtpControlEx *ftp)
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
	} while( !strcmp(".", ftp->findinfo.Data.name) || !strcmp("..", ftp->findinfo.Data.name) );
	return fill_dentry(ftp->findinfo);
}

int ftp_go_somewhere(CFtpControlEx *ftp, std::string olddir)
{
	ftp->do_list = true;
	SAFE_FTP_CALL(
		if( ftp->ChDir(ftp->curpath.c_str()) ) return 1;
		else {
			ftp->curpath = olddir;
			return -1;
		}
	)
}

int ftp_goparent_fn(CFtpControlEx *ftp)
{
	if( ftp->curpath.size() == 1 )
		return -1;
	std::string olddir = ftp->curpath;
	ftp->curpath.resize(ftp->curpath.rfind("/", ftp->curpath.size()-2)+1);
	return ftp_go_somewhere(ftp, olddir);
}

int ftp_gosibling_fn(char *name, CFtpControlEx *ftp)
{
	if( ftp->curpath.size() == 1 )
		return -1;
	std::string olddir = ftp->curpath;
	ftp->curpath.resize(ftp->curpath.rfind("/", ftp->curpath.size()-2)+1);
	ftp->curpath += name;
	ftp->curpath += "/";
	return ftp_go_somewhere(ftp, olddir);
}

int ftp_gochild_fn(char *name, CFtpControlEx *ftp)
{
	std::string olddir = ftp->curpath;
	ftp->curpath += name;
	ftp->curpath += "/";
	return ftp_go_somewhere(ftp, olddir);
}

static struct dt_walker walker = {
    (dt_init_fn) &ftp_init_fn,
    (dt_fini_fn) &ftp_fini_fn,
    (dt_readdir_fn) &ftp_readdir_fn,
    (dt_goparent_fn) &ftp_goparent_fn,
    (dt_gosibling_fn) &ftp_gosibling_fn,
    (dt_gochild_fn) &ftp_gochild_fn
};

//////////////////////////////////////////////////////////////////////////
// main
//////////////////////////////////////////////////////////////////////////

static void usage(char *binname, int err)
{
	//todo: this should be rewritten
    fprintf(stderr, "Usage: %s [-f out] host_ip\n", binname);
    fprintf(stderr, "out:\n");
    fprintf(stderr, "\tfull - print full paths\n");
    fprintf(stderr, "\tsimplified - print only id of a path\n");
    fprintf(stderr, "\tfilesfirst - simplified with filis printed first\n");
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

    struct dt_dentry d = {DT_DIR, strdup(""), 0, NULL, NULL, NULL, 0};
    CFtpControlEx curdir;
    dt_out out = DT_OUT_SIMPLIFIED;
    char *host;

    CFtpControl::DefaultAnsiCP = "cp1251";

    //todo: this should be rewritten
    if (argc < 2)
        usage(argv[0], EXIT_FAILURE);
    
    host = argv[1];

    if (strcmp(argv[1], "-f") == 0) {
        if (argc < 4)
            usage(argv[0], EXIT_FAILURE);

        host = argv[3];
        if (strcmp(argv[2], "full") == 0)
            out = DT_OUT_FULL;
        else if (strcmp(argv[2], "simplified") == 0)
            out = DT_OUT_SIMPLIFIED;
        else if (strcmp(argv[2], "reverse") == 0)
            out = DT_OUT_REVERSE;
        else {
            fprintf(stderr, "Unknown output format\n");
            usage(argv[0], EXIT_FAILURE);
        }
    }

	curdir.ServerIP = inet_addr(host);
	//curdir.ServerPORT;
	//curdir.login;
	//curdir.pass;
	//curdir.timeouts;
	//curdir.SetConnCP(const char *cpId);

	try {
		switch(out) {
			case DT_OUT_REVERSE:
				dt_singlewalk(&walker, &d, &curdir, out);
				break;
			default:
				dt_mktree(&walker, &d, &curdir, out);
				dt_printtree(&d, out);
				dt_free(&d);
				break;
		}
	} catch(const CFtpControl::NetworkError&){}

    return 0;
}

