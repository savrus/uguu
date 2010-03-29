/* sqlscan - sql scanner (for database dump)
 *
 * Copyright 2009, 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "dt.h"
#include "sqlwk.h"
#include "estat.h"
#include "buf.h"

#define MAX_PASSWORD_LENGTH 100

static void usage(char *binname, int err)
{
    fprintf(stderr, "Usage: %s [-l] [-f] [-s protocol] [-p port] "
        "[-dh db_host] [-dp db_port] [-du db_user] [-dd db_name] [-dP] "
        "host\n", binname);
    fprintf(stderr, "  -l\tlookup mode (detect if there is anything available)\n");
    fprintf(stderr, "  -f\tprint full paths (debug output)\n");
    fprintf(stderr, "  -s\tspecify share protocol, default: \"smb\"\n");
    fprintf(stderr, "  -p\tspecify share port, default: 0\n");
    fprintf(stderr, "  -dh\tdatabase host\n");
    fprintf(stderr, "  -dp\tdatabase port\n");
    fprintf(stderr, "  -du\tdatabase user, default: \"postgres\"\n");
    fprintf(stderr, "  -dd\tdatabase name, default: \"uguu\"\n");
    fprintf(stderr, "  -dP\tprompt for database password\n");
    exit(err);
}

int main(int argc, char **argv)
{
    struct dt_dentry d = {DT_DIR, "", 0, NULL, NULL, NULL, 0};
    struct dt_dentry *probe;
    struct sqlwk_dir curdir;
    int full = 0;
    int lookup = 0;
    char *proto = "smb";
    char *host;
    unsigned int port = 0;
    char *dbhost = NULL;
    char *dbport = NULL;
    char *dbuser = "postgres";
    char *dbname = "uguu";
    char dbpass[MAX_PASSWORD_LENGTH];
    struct termios tset, torig;
    struct buf_str *bs;
    int i;
    int ret;
    
    dbpass[0] = 0;

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
            case 'p':
                i++;
                if ((i >= argc) ||(sscanf(argv[i], "%u", &port) == 0))
                    usage(argv[0], ESTAT_FAILURE);
                break;
            case 's':
                i++;
                if (i >= argc)
                    usage(argv[0], ESTAT_FAILURE);
                proto = argv[i];
                break;
            case 'd':
                switch (argv[i][2]) {
                    case 'h':
                        i++; dbhost = argv[i]; break;
                    case 'p':
                        i++; dbport = argv[i]; break;
                    case 'u':
                        i++; dbuser = argv[i]; break;
                    case 'd':
                        i++; dbname = argv[i]; break;
                    case 'P':
                        printf("Password: ");
                        fflush(stdout);
                        tcgetattr(fileno(stdin), &torig);
                        tset = torig;
                        tset.c_lflag &= ~ECHO;
                        tcsetattr(fileno(stdin), TCSADRAIN, &tset);
                        if (fgets(dbpass, MAX_PASSWORD_LENGTH, stdin) == NULL)
                            exit(ESTAT_FAILURE);
                        tcsetattr(fileno(stdin), TCSADRAIN, &torig);
                        if ((strlen(dbpass) == 0)
                            || (dbpass[strlen(dbpass)-1] != '\n')) {
                            fprintf(stderr, "Too long or bad password");
                            exit(ESTAT_FAILURE);
                        }
                        dbpass[strlen(dbpass)-1] = 0;
                        printf("\n");
                        break;
                    default:
                        usage(argv[0], ESTAT_FAILURE);
                }
                if (i >= argc)
                    usage(argv[0], ESTAT_FAILURE);
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

    if ((bs = buf_alloc()) == NULL)
        exit(ESTAT_FAILURE);

    ret = ! buf_appendf(bs, "user='%s' dbname='%s'", dbuser, dbname);
    if (dbhost != NULL)
        ret += ! buf_appendf(bs, " host='%s'", dbhost);
    if (dbport != NULL)
        ret += ! buf_appendf(bs, " port='%s'", dbport);
    if (*dbpass != 0)
        ret += ! buf_appendf(bs, " password='%s'", dbpass);
    if (ret)
        exit(ESTAT_FAILURE);

    if (sqlwk_open(&curdir, proto, host, port, buf_string(bs)) < 0)
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

