/* sqlwk - sql walker for 'dir tree' engine (database dump)
 *
 * Copyright 2009, 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#include <stdlib.h>
#include <libpq-fe.h>
#include <string.h>

#include "dt.h"
#include "estat.h"
#include "sqlwk.h"
#include "log.h"
#include "buf.h"

static int sqlwk_query(struct sqlwk_dir *c, const char *query, ...)
{
    struct buf_str *bs;
    va_list ap;
    PGresult *result;

    if ((bs = buf_alloc()) == NULL)
        return -1;
    
    va_start(ap, query);
    buf_vappendf(bs, query, ap);
    if (buf_error(bs)) {
        buf_free(bs);
        return -1;
    }
    va_end(ap);

    if (c->res != NULL) {
        PQclear(c->res);
        c->res = NULL;
        c->rows = 0;
    }

    //printf("%s\n", buf_string(bs));
    result = PQexec(c->conn, buf_string(bs));
    
    if ((result == NULL)
        || ((PQresultStatus(result) != PGRES_TUPLES_OK)
            && (PQresultStatus(result) != PGRES_COMMAND_OK))) {
        LOG_ERR("PGexec() returned bad result for query '%s' (%s)\n",
            buf_string(bs),
            (result == NULL) ? "NULL" : PQresultErrorMessage(result));
        buf_free(bs);
        return -1;
    }
    buf_free(bs);
    
    c->res = result;
    c->cur_row = 0;
    c->rows = PQntuples(result);

    return c->rows;
}

static struct buf_str * sqlwk_escape(const char *s)
{
    struct buf_str *bs;
    char *sc;

    if ((bs = buf_alloc()) == NULL)
        return NULL;

    while ((sc = strpbrk(s, "\\\'")) != NULL) {
        char c = *sc;
        buf_appendn(bs, s, (size_t) sc - (size_t) s);
        buf_appendf(bs, "\\%c", c);
        s = sc + 1;
    }
    buf_append(bs, s);

    if (buf_error(bs)) {
        buf_free(bs);
        return NULL;
    }

    return bs;
}

static int sqlwk_query_opendir(struct sqlwk_dir *c)
{
    /* dir ids are assigned inside 'dt' in the order items are
     * received from host. So we sort by sharedir_id here to preserve
     * that ordering */
    return sqlwk_query(c, "SELECT sharedir_id AS dirid, size, name "
        "FROM files "
        "WHERE share_id = %llu AND sharepath_id = %llu "
        "ORDER BY sharedir_id;", c->share_id, c->sharepath_id );
}

static int sqlwk_query_parent(struct sqlwk_dir *c)
{
    int ret;
    unsigned long long id;
    ret = sqlwk_query(c, "SELECT parent_id FROM paths "
        "WHERE share_id = %llu AND sharepath_id = %llu;",
        c->share_id, c->sharepath_id);
    if (ret == -1)
        return -1;
    if (PQgetisnull(c->res, 0, 0))
        return -1;
    if (sscanf(PQgetvalue(c->res, 0, 0), "%llu", &id) == 0)
        return -1;
    c->sharepath_id = id;
    return ret;
}

static int sqlwk_query_child(struct sqlwk_dir *c, const char *name)
{
    int ret;
    unsigned long long id;
    struct buf_str *bs;

    if ((bs = sqlwk_escape(name)) == NULL) {
        printf ("got NULL %s\n", name);
        return -1;
    }
    ret = sqlwk_query(c, "SELECT sharedir_id FROM files WHERE "
        "share_id = %llu AND sharepath_id = %llu "
        "AND name = E'%s';",
        c->share_id, c->sharepath_id, buf_string(bs));
    buf_free(bs);
    if (ret == -1)
        return -1;
    if (PQgetisnull(c->res, 0, 0))
        return -1;
    if (sscanf(PQgetvalue(c->res, 0, 0), "%llu", &id) == 0)
        return -1;
    c->sharepath_id = id;
    return ret;
}


int sqlwk_open(struct sqlwk_dir *c, const char *proto, const char *host, unsigned int port, const char *conninfo)
{
    int ret;

    c->conn = PQconnectdb(conninfo);
    c->res = NULL;
    
    if (c->conn == NULL) {
        LOG_ERR("PQconnectdb() returned NULL\n");
        goto err;
    }
    
    if (PQstatus(c->conn) != CONNECTION_OK) {
        LOG_ERR("Unable to connect to database\n");
        goto clean_conn;
    }

    ret = sqlwk_query(c, "SELECT share_id FROM shares WHERE protocol = '%s'"
                         " AND hostname = '%s' AND port = %d;",
                         proto, host, port);
    if (ret <= 0) {
        LOG_ERR("Could not filnd  %s://%s:%d in the shares table\n", proto, host, port);
        goto clean_conn;
    }

    if (PQgetisnull(c->res, 0, 0)) {
        LOG_ERR("PQgetisnull() reports share_id is NULL\n");
        goto clean_conn;
    }

    if (sscanf(PQgetvalue(c->res, 0, 0), "%llu", &(c->share_id)) == 0) {
        LOG_ERR("sscanf() couldn't read share_id\n");
        goto clean_conn;
    }
    
    c->sharepath_id = 1;
    
    if (sqlwk_query_opendir(c) == -1)
        goto clean_conn;
    
    return 1;

clean_conn:
    PQfinish(c->conn);
err:
    return -1;
}

int sqlwk_close(struct sqlwk_dir *c)
{
    if (c->res != NULL)
        PQclear(c->res);

    PQfinish(c->conn);

    return 1;
}

struct dt_dentry * sqlwk_readdir(void *curdir)
{
    struct sqlwk_dir *c = (struct sqlwk_dir*) curdir;
    struct dt_dentry *d;
    unsigned long long size;
    unsigned long long sharedir_id;
    unsigned long row;
   
    if (c->cur_row >= c->rows)
        return NULL;
    row = c->cur_row;
    c->cur_row++;

    if (PQgetisnull(c->res, row, 0)
        ||PQgetisnull(c->res, row, 1)
        ||PQgetisnull(c->res, row, 2)) {
        LOG_ERR("Some results in current row are NULL\n");
        return NULL;
    }

    if (sscanf(PQgetvalue(c->res, row, 0), "%llu", &sharedir_id) == 0) {
        LOG_ERR("Bad query results: couldn't read sharedir_id\n");
        return NULL;
    }
    if (sscanf(PQgetvalue(c->res, row, 1), "%llu", &size) == 0) {
        LOG_ERR("Bad query results: couldn't read size\n");
        return NULL;
    }
    
    d = dt_alloc();
    if (d == NULL) {
        LOG_ERR("dt_alloc() returned NULL\n");
        return NULL;
    }
    d->name = strdup(PQgetvalue(c->res, row, 2));
    if (sharedir_id > 0)
        d->type = DT_DIR;
    else {
        d->type = DT_FILE;
        d->size = size;
    }
    
    return d;
}

static int sqlwk_go(dt_go type, char *name, void *curdir)
{
    struct sqlwk_dir *c = (struct sqlwk_dir*) curdir;

    switch (type) {
        case DT_GO_PARENT:
            if (sqlwk_query_parent(c) == -1)
                return -1;
            break;
        case DT_GO_CHILD:
            if (sqlwk_query_child(c, name) == -1)
                return -1;

            /* 'dir tree' only requires this for go_child */
            if (sqlwk_query_opendir(c) == -1)
                return -1;
            break;
        default:
            LOG_ERR("unknown smbwk_go_type %d, sharepath_id: %llu\n", type, c->sharepath_id);
            return -1;
    }
   
    return 1;
}

struct dt_walker sqlwk_walker = {
    sqlwk_readdir,
    sqlwk_go,
};

