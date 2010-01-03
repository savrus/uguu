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
    int out = DT_OUT_SIMPLIFIED;
    char *host;

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

    smbwk_init_curdir(&curdir, host);
    
    switch(out) {
        case DT_OUT_REVERSE:
            dt_singlewalk(&smbwk_walker, &d, &curdir, out);
            break;
        default:
            dt_mktree(&smbwk_walker, &d, &curdir, out);
            dt_printtree(&d, out);
            dt_free(&d);
            break;
    }

    smbwk_fini_curdir(&curdir);

    return 0;
}

