/* webdavscan - WebDAV scanner
 *
 * Copyright 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <neon/ne_socket.h>
#include <neon/ne_utils.h>

#include "dt.h"
#include "wdwk.h"
#include "estat.h"
#include "log.h"

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

static void usage(char *binname, int err)
{
    fprintf(stderr, "Usage: %s [-l] [-f | -u oldtree] [-p##] host\n", binname);
    fprintf(stderr, "  -l\tlookup mode (detect if there is anything available)\n");
    fprintf(stderr, "  -f\tprint full paths (debug output)\n");
    fprintf(stderr, "  -p##\tremote port\n");
    fprintf(stderr, "  -u\tdiff against an old tree\n");
    exit(err);
}

int main(int argc, char **argv)
{
    struct dt_dentry *probe;
    struct wdwk_dir curdir;
    int full = 0;
    int lookup = 0;
    char *host;
    int i;
    char *oldtree = NULL;
    FILE *oldfile;
    int port = 0;

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
            case 'p':
                port = atoi(&argv[i][2]);
                break;
            case 'l':
                lookup = 1;
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

    ne_debug_init(stderr, 0);
    
    if (ne_sock_init()) {
        LOG_ERR("ne_sock_init() returned non-zero\n");
        exit(ESTAT_NOCONNECT);
    }

    host = argv[i];

    if (wdwk_open(&curdir, host, port) < 0)
        exit(ESTAT_NOCONNECT);

    if (lookup) {
        probe = wdwk_walker.readdir(&curdir);
        wdwk_close(&curdir);
        if (probe != NULL) {
            dt_free(probe);
            exit(ESTAT_SUCCESS);
        }
        else
            exit(ESTAT_FAILURE);
    }

    if (full)
        dt_full(&wdwk_walker, &curdir);
    else if (oldtree) {
        if ((oldfile = fopen(oldtree, "r")) == NULL) {
            LOG_ERRNO("Can't open file %s\n", oldtree);
            exit(ESTAT_FAILURE);
        }
        dt_diff(oldfile, &wdwk_walker, &curdir);
        fclose(oldfile);
    } else
        dt_reverse(&wdwk_walker, &curdir);

    wdwk_close(&curdir);
    ne_sock_exit();

    return ESTAT_SUCCESS;
}

