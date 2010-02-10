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

#include "dt.h"
#include "smbwk.h"

static void usage(wchar_t *binname, int err)
{
    wchar_t *bin = wcsrchr(binname, L'\\');
    if (bin) binname = bin + 1;
	setlocale(LC_ALL, ".OCP");
	fprintf(stderr, "Usage: %S [-l] [-f] [-u username] [-p password] [-a|-d] [-h] host_ip\n", binname);
	fprintf(stderr, "\t-l\tlookup mode (detect if there is anything available)\n");
	fprintf(stderr, "\t-f\tprint full paths (debug output)\n");
	fprintf(stderr, "\t-a\tskip admin shares\n");
	fprintf(stderr, "\t-d\tskip shares with trailing dollar (hidden)\n");
	fprintf(stderr, "\t-h\tprint this help\n");
	exit(err);
}

int wmain(int argc, wchar_t **argv)
{
    struct dt_dentry d = {DT_DIR, "", 0, NULL, NULL, NULL, 0}, *probe;
    struct smbwk_dir curdir;
    int full = 0, lookup = 0;
    wchar_t *host, *user = L"Guest", *pass = L"";
    enum_type etype = ENUM_ALL;
    int i;

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
                user = argv[++i];
                break;
            case L'p':
                pass = argv[++i];//TODO: read from stdin
                break;
            case L'a':
            case L'd':
                if (ENUM_ALL != etype)
                    usage(argv[0], EXIT_FAILURE);
                etype = (L'a'==argv[i][1]) ? ENUM_SKIP_ADMIN : ENUM_SKIP_DOLLAR;
                break;
            case L'h':
                usage(argv[0], EXIT_SUCCESS);
            default:
                usage(argv[0], EXIT_FAILURE);
        }
    }

    if (i+1 != argc)
        usage(argv[0], EXIT_FAILURE);
    
    host = argv[i];

    if (smbwk_open(&curdir, host, user, pass, etype) < 0)
        return EXIT_FAILURE;

    if (lookup) {
        if ((probe = smbwk_walker.readdir(&curdir)) != NULL)
            dt_free(probe);
        smbwk_close(&curdir);
        return probe ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    
    if (full)
        dt_full(&smbwk_walker, &d, &curdir);
    else
        dt_reverse(&smbwk_walker, &d, &curdir);

    smbwk_close(&curdir);

    return EXIT_SUCCESS;
}

