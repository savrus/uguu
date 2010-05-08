/* smbscan - smb scanner
 *
 * Copyright 2009, 2010, savrus
 * Copyright 2010, Radist <radist.nt@gmail.com>
 * Read the COPYING file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <crtdbg.h>
#include <locale.h>
#include <Windows.h>

#include "dt.h"
#include "smbwk.h"
#include "estat.h"
#include "log.h"
#include "getpass.h"

#define MAX_PASSWORD_LEN 100

static void usage(wchar_t *binname, int err)
{
    wchar_t *bin = wcsrchr(binname, L'\\');
    if (bin) binname = bin + 1;
	fprintf(stderr, "Usage: %S [-l] [-f|-u ot] [-U username] [-P] [-a|-d] [-h] host_ip\n", binname);
	fprintf(stderr, "  -l\tlookup mode (detect if there is anything available)\n");
	fprintf(stderr, "  -f\tprint full paths (debug output)\n");
	fprintf(stderr, "  -u ot\tdiff against an old tree\n");
	fprintf(stderr, "  -a\tskip admin shares\n");
	fprintf(stderr, "  -d\tskip shares with trailing dollar (hidden)\n");
	fprintf(stderr, "  -P\tfirst line (ends with newline char) in stdin is utf8-encoded password");
	fprintf(stderr, "  -h\tprint this help\n");
	exit(err);
}

int wmain(int argc, wchar_t **argv)
{
    struct dt_dentry d = {DT_DIR, "", 0, NULL, NULL, NULL, 0}, *probe;
    struct smbwk_dir curdir;
    int full = 0, lookup = 0;
    wchar_t *host, *user = L"Guest", wpass[MAX_PASSWORD_LEN+1] = L"", *oldtree = NULL;
    FILE *oldfile;
    enum_type etype = ENUM_ALL;
    int i;

	setlocale(LC_ALL, ".OCP");
#if defined(_DEBUG)
	_CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_DEBUG );
	_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_DEBUG );
	_CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_DEBUG );
	_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF | _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG));
#endif // _DEBUG

    for (i = 1; i < argc; i++) {
        if (argv[i][0] != L'-')
            break;
        if (argv[i][1] == L'-' && argv[i][2] == 0) {
            i++;
            break;
        }
        switch (argv[i][1]) {
            case L'f':
                full = 1;
                break;
            case L'l':
                lookup = 1;
                break;
            case L'u':
                oldtree = argv[++i];
                break;
            case L'U':
                user = argv[++i];
                break;
            case L'P': {
                    char pass[MAX_PASSWORD_LEN+2];
                    size_t passlen;
                    if ((passlen = gp_readline(pass, MAX_PASSWORD_LEN+2)) == 0) {
                        LOG_ERR("Empty, not completed, too long password or input error\n");
                        usage(argv[0], ESTAT_FAILURE);
                    }
                    if (!MultiByteToWideChar(CP_UTF8, 0, pass, -1, wpass, MAX_PASSWORD_LEN+1)) {
                        LOG_ERR("Invalid utf-8 chars in password\n");
                        usage(argv[0], ESTAT_FAILURE);
                    }
                break;
                }
            case L'a':
            case L'd':
                if (ENUM_ALL != etype)
                    usage(argv[0], ESTAT_FAILURE);
                etype = (L'a'==argv[i][1]) ? ENUM_SKIP_ADMIN : ENUM_SKIP_DOLLAR;
                break;
            case L'h':
                usage(argv[0], ESTAT_SUCCESS);
            default:
                usage(argv[0], ESTAT_FAILURE);
        }
    }

    if (i+1 != argc || (full && oldtree))
        usage(argv[0], ESTAT_FAILURE);
    host = argv[i];

    if ((i = smbwk_open(&curdir, host, user, wpass, etype)) != ESTAT_SUCCESS)
        return i;

    if (lookup) {
        if ((probe = smbwk_walker.readdir(&curdir)) != NULL)
            dt_free(probe);
        smbwk_close(&curdir);
        return probe ? ESTAT_SUCCESS : ESTAT_FAILURE;
    }
    
    if (full)
        dt_full(&smbwk_walker, &d, &curdir);
    else if (oldtree) {
        if (_wfopen_s(&oldfile, oldtree, L"rt") != 0) {
            LOG_ERRNO("Can't open file %S\n", oldtree);
            usage(argv[0], ESTAT_FAILURE);
        }
        dt_diff(oldfile, &smbwk_walker, &d, &curdir);
        fclose(oldfile);
    } else
        dt_reverse(&smbwk_walker, &d, &curdir);

    smbwk_close(&curdir);

    return ESTAT_SUCCESS;
}

