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

#include "dt.h"
#include "smbwk.h"

static void usage(wchar_t *binname, int err)
{
    fprintf(stderr, "Usage: %S [-f] host\n", binname);
    fprintf(stderr, "\t-f\tprint full paths (debug output)\n");
    exit(err);
}

int wmain(int argc, wchar_t **argv)
{
    struct dt_dentry d = {DT_DIR, "", 0, NULL, NULL, NULL, 0};
    struct smbwk_dir curdir;
    int full = 0;
    wchar_t *host, *user = NULL, *pass = NULL;

#if defined(_DEBUG)
	_CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_DEBUG );
	_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_DEBUG );
	_CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_DEBUG );
	_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF | _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG));
#endif // _DEBUG

    if (argc < 2)
        usage(argv[0], EXIT_FAILURE);
    
    host = argv[1];

    if (wcscmp(argv[1], L"-f") == 0) {
        if (argc < 3)
            usage(argv[0], EXIT_FAILURE);
        full = 1;
        host = argv[2];
    }

	if (!user)
		user = L"Guest";
	if (!pass)
		pass = L"";

    if (smbwk_open(&curdir, host, user, pass, ENUM_SKIP_DOLLAR) < 0)
        return EXIT_FAILURE;
    
    if (full)
        dt_full(&smbwk_walker, &d, &curdir);
    else
        dt_reverse(&smbwk_walker, &d, &curdir);

    smbwk_close(&curdir);

    return 0;
}

