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
    fprintf(stderr, "Usage: %s [-l] [-f] host\n", binname);
    fprintf(stderr, "  -l\tlookup mode (detect if there is anything available)\n");
    fprintf(stderr, "  -f\tprint full paths (debug output)\n");
    exit(err);
}

int main(int argc, char **argv)
{
    struct dt_dentry d = {DT_DIR, "", 0, NULL, NULL, NULL, 0};
    struct smbwk_dir curdir;
    int full = 0;
    int lookup = 0;
    char *host;
    int i;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-')
            break;
        switch (argv[i][1]) {
            case 'f':
                full = 1;
                break;
            case 'l':
                lookup = 1;
                break;
        }
    }
    
    if (i == argc)
        usage(argv[0], EXIT_FAILURE);

    host = argv[i];

    if (smbwk_open(&curdir, host) < 0)
        exit(EXIT_FAILURE);

    if (lookup) {
        if (smbwk_walker.readdir(&curdir) != NULL)
            exit(EXIT_SUCCESS);
        else
            exit(EXIT_FAILURE);
    }

    if (full)
        dt_full(&smbwk_walker, &d, &curdir);
    else
        dt_reverse(&smbwk_walker, &d, &curdir);

    smbwk_close(&curdir);

    return 0;
}

