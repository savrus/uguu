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
    struct dt_dentry *probe;
    struct smbwk_dir curdir;
    int full = 0;
    int lookup = 0;
    char *host;
    int i;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-')
            break;
        if (argv[i][1] == '-' && argv[i][2] == 0) {
            i++;
            break;
        }
        switch (argv[i][1]) {
            case 'f':
                full = 1;
                break;
            case 'l':
                lookup = 1;
                break;
            case 'h':
                usage(argv[0], EXIT_SUCCESS);
            default:
                usage(argv[0], EXIT_FAILURE);
        }
    }
    
    if (i + 1 != argc)
        usage(argv[0], EXIT_FAILURE);

    host = argv[i];

    if (smbwk_open(&curdir, host) < 0)
        exit(EXIT_FAILURE);

    if (lookup) {
        probe = smbwk_walker.readdir(&curdir);
        dt_free(probe);
        smbwk_close(&curdir);
        return (probe != NULL) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (full)
        dt_full(&smbwk_walker, &d, &curdir);
    else
        dt_reverse(&smbwk_walker, &d, &curdir);

    smbwk_close(&curdir);

    return EXIT_SUCCESS;
}

