/* smbscan - smb scanner
 *
 * Copyright 2009, 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dt.h"
#include "smbwk.h"

static void usage(char *binname, int err)
{
    fprintf(stderr, "Usage: %s [-f out] host\n", binname);
    fprintf(stderr, "out:\n");
    fprintf(stderr, "\tfull - print full paths\n");
    fprintf(stderr, "\tsimplified - print only id of a path\n");
    fprintf(stderr, "\tfilesfirst - simplified with filis printed first\n");
    exit(err);
}

int main(int argc, char **argv)
{
    struct dt_dentry d = {DT_DIR, "", 0, NULL, NULL, NULL, 0};
    struct smbwk_dir curdir;
    int full = 0;
    char *host;

    if (argc < 2)
        usage(argv[0], EXIT_FAILURE);
    
    host = argv[1];

    if (strcmp(argv[1], "-f") == 0) {
        if (argc < 3)
            usage(argv[0], EXIT_FAILURE);
        full = 1;
        host = argv[2];
    }

    if (smbwk_open(&curdir, host) < 0)
        exit(EXIT_FAILURE);
    
    if (full)
        dt_full(&smbwk_walker, &d, &curdir);
    else
        dt_reverse(&smbwk_walker, &d, &curdir);

    smbwk_close(&curdir);

    return 0;
}

