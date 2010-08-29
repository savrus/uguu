/* curlftpscan - FTP scanner based on CURL library
 *
 * Copyright 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include "dt.h"
#include "cfwk.h"
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
    fprintf(stderr, "  -C cp\tforce server codepage (without detecting utf8)\n");
    fprintf(stderr, "  -p##\tremote port\n");
    fprintf(stderr, "  -u\tdiff against an old tree\n");
    exit(err);
}

int main(int argc, char **argv)
{
    struct dt_dentry *probe;
    struct cfwk_dir curdir;
    int full = 0;
    int lookup = 0;
    char *host;
    int i;
    char *oldtree = NULL;
    FILE *oldfile;
    char *port = NULL;
    char *server_cp = NULL;
    CURLcode rcode;

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
                port = &argv[i][2];
                break;
            case 'l':
                lookup = 1;
                break;
            case 'C':
                i++;
                if (i >= argc)
                    usage(argv[0], ESTAT_FAILURE);
                server_cp = argv[i];
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

    if ((rcode = curl_global_init(CURL_GLOBAL_NOTHING)) != CURLE_OK) {
        LOG_ERR("curl_global_init() returned non-zero: %s\n",
                curl_easy_strerror(rcode));
        exit(ESTAT_FAILURE);
    }

    host = argv[i];

    if (cfwk_open(&curdir, host, port, server_cp) < 0)
        exit(ESTAT_NOCONNECT);

    if (lookup) {
        probe = cfwk_walker.readdir(&curdir);
        cfwk_close(&curdir);
        if (probe != NULL) {
            dt_free(probe);
            exit(ESTAT_SUCCESS);
        }
        else
            exit(ESTAT_FAILURE);
    }

    if (full)
        dt_full(&cfwk_walker, &curdir);
    else if (oldtree) {
        if ((oldfile = fopen(oldtree, "r")) == NULL) {
            LOG_ERRNO("Can't open file %s\n", oldtree);
            exit(ESTAT_FAILURE);
        }
        dt_diff(oldfile, &cfwk_walker, &curdir);
        fclose(oldfile);
    } else
        dt_reverse(&cfwk_walker, &curdir);

    cfwk_close(&curdir);
    curl_global_cleanup();

    return ESTAT_SUCCESS;
}

