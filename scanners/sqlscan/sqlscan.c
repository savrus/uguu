/* sqlscan - sql scanner (for database dump)
 *
 * Copyright 2009, 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dt.h"
#include "sqlwk.h"
#include "estat.h"

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
    struct sqlwk_dir curdir;
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
                usage(argv[0], ESTAT_SUCCESS);
            default:
                usage(argv[0], ESTAT_FAILURE);
        }
    }
    
    if (i + 1 != argc)
        usage(argv[0], ESTAT_FAILURE);

    host = argv[i];

    if (sqlwk_open(&curdir, host, "user=postgres dbname=uguu") < 0)
        exit(ESTAT_FAILURE);

    if (lookup) {
        probe = sqlwk_walker.readdir(&curdir);
        sqlwk_close(&curdir);
        if (probe != NULL) {
            dt_free(probe);
            exit(ESTAT_SUCCESS);
        }
        else
            exit(ESTAT_FAILURE);
    }

    if (full)
        dt_full(&sqlwk_walker, &d, &curdir);
    else
        dt_reverse(&sqlwk_walker, &d, &curdir);

    sqlwk_close(&curdir);

    return ESTAT_SUCCESS;
}

