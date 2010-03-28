/* sqlwk - definitions and interfaces of sql walker
 *
 * Copyright 2009, 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#ifndef SQLWK_H
#define SQLWK_H

#include <libpq-fe.h>
#include "dt.h"

struct sqlwk_dir {
    PGconn *conn;
    PGresult *res;
    unsigned long cur_row;
    unsigned long rows;
    unsigned long long share_id;
    unsigned long long sharepath_id;
};

/* initialise the walker */
int sqlwk_open(struct sqlwk_dir *c, char *host, const char *conninfo);
/* close the walker */
int sqlwk_close(struct sqlwk_dir *c);

/* sql walker for 'dir tree' engine */
extern struct dt_walker sqlwk_walker;

#endif /* SQLWK_H */
