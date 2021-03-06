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
#include "estat.h"
#include "log.h"

static void usage(char *binname, int err)
{
    fprintf(stderr, "Usage: %s [-l] [-f] [-a|-d] host\n", binname);
    fprintf(stderr, "  -l\tlookup mode (detect if there is anything available)\n");
    fprintf(stderr, "  -a\tskip 'admin shares' in root directory\n");
    fprintf(stderr, "  -d\tskip files ended with bucks in root directory\n");
    fprintf(stderr, "  -f\tprint full paths (debug output)\n");
    fprintf(stderr, "  -u\tdiff against an old tree\n");
    exit(err);
}

int main(int argc, char **argv)
{
    struct dt_dentry *probe;
    struct smbwk_dir curdir;
    int full = 0;
    int lookup = 0;
    int skip_bucks = SKIP_BUCKS_NONE;
    char *host;
    int i;
    char *oldtree = NULL;
    FILE *oldfile;

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
            case 'a':
            case 'd':
                if (skip_bucks != SKIP_BUCKS_NONE)
                    usage(argv[0], ESTAT_FAILURE);
                skip_bucks = (argv[i][1] == 'a') ? SKIP_BUCKS_ADMIN
                                                 : SKIP_BUCKS_ALL;
                break;
            case 'u':
                i++;
                if (i >= argc)
                    usage(argv[0], ESTAT_FAILURE);
                oldtree = argv[i];
                break;
            case 'h':
                usage(argv[0], ESTAT_SUCCESS);
            default:
                usage(argv[0], ESTAT_FAILURE);
        }
    }
    
    if (i + 1 != argc)
        usage(argv[0], ESTAT_FAILURE);

    host = argv[i];

    if (smbwk_open(&curdir, host, skip_bucks) < 0)
        exit(ESTAT_NOCONNECT);

    if (lookup) {
        probe = smbwk_walker.readdir(&curdir);
        smbwk_close(&curdir);
        if (probe != NULL) {
            dt_free(probe);
            exit(ESTAT_SUCCESS);
        }
        else
            exit(ESTAT_FAILURE);
    }

    if (full)
        dt_full(&smbwk_walker, &curdir);
    else if (oldtree) {
        if ((oldfile = fopen(oldtree, "r")) == NULL) {
            LOG_ERRNO("Can't open file %s\n", oldtree);
            return ESTAT_FAILURE;
        }
        dt_diff(oldfile, &smbwk_walker, &curdir);
        fclose(oldfile);
    } else
        dt_reverse(&smbwk_walker, &curdir);

    smbwk_close(&curdir);

    return ESTAT_SUCCESS;
}

