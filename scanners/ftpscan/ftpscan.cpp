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

//////////////////////////////////////////////////////////////////////////
// dt_walker routines

class CFtpControlEx
: public CFtpControl
{
public:
	CFtpControlEx() : curpath("/"), do_list(true), errors(10) {}
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

static void TryReconnect(CFtpControlEx *ftp)
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

static struct dt_dentry *fill_dentry(FtpFindInfo &fi)
{
	struct dt_dentry *result = (struct dt_dentry*)calloc(1, sizeof(struct dt_dentry));
	result->type = fi.Data.flagtrycwd?DT_DIR:DT_FILE;
	result->name = strdup(fi.Data.name);
	if( DT_FILE == result->type )
		result->size = fi.Data.size;
	return result;
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
	} while( !strcmp(".", ftp->findinfo.Data.name) || !strcmp("..", ftp->findinfo.Data.name) );
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

static void usage(char *binname, int err)
{
	//todo: this should be rewritten
    fprintf(stderr, "Usage: %s [-f] host_ip\n", binname);
    fprintf(stderr, "\t-f\tprint full paths (debug output)\n");
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

    struct dt_dentry d = {DT_DIR, "", 0, NULL, NULL, NULL, 0};
    CFtpControlEx curdir;
    bool full = false;
    char *host;

    CFtpControl::DefaultAnsiCP = "cp1251";

    //todo: this should be rewritten
    if (argc < 2)
        usage(argv[0], EXIT_FAILURE);
    
    host = argv[1];

    if (strcmp(argv[1], "-f") == 0) {
        if (argc < 3)
            usage(argv[0], EXIT_FAILURE);

        host = argv[2];
		full = true;
    }

	curdir.ServerIP = inet_addr(host);
	//curdir.ServerPORT;
	//curdir.login;
	//curdir.pass;
	//curdir.timeouts;
	//curdir.SetConnCP(const char *cpId);

	try {
		TryReconnect(&curdir);
		if (full)
			dt_full(&walker, &d, &curdir);
		else
			dt_reverse(&walker, &d, &curdir);
	} catch(const CFtpControl::NetworkError&){}
	curdir.Quit();

    return 0;
}

