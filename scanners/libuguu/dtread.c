/* dtread - read 'reversed' dt ouput and reconstruct tree
 *
 * Copyright 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#include <stdio.h>
#include <string.h>

#include "log.h"
#include "dt.h"
#include "buf.h"
#include "cuckoo.h"

#ifdef _MSC_VER
#define strdup _strdup
#endif

struct dtread_data {
    struct dt_dentry *de;
    struct dt_dentry *child;
    struct dt_dentry *file_child;
};

static struct dtread_data * dtread_data_alloc()
{
    struct dtread_data *dr;
    dr = (struct dtread_data *) calloc(1, sizeof(struct dtread_data));
    if (dr == NULL){
        LOG_ERR("calloc() returned NULL\n");
        return NULL;
    }
    if ((dr->de = dt_alloc()) == NULL){
        LOG_ERR("dt_alloc() returned NULL\n");
        free(dr);
        return NULL;
    }
    return dr;
}

static void dtread_data_free(void *ptr)
{
    struct dtread_data *dr = (struct dtread_data *) ptr;
    dt_rfree(dr->de);
    free(dr);
}

static int dtread_readline(const char *line, struct cuckoo_ctx *cu, unsigned int *maxid)
{
    struct dtread_data *dr, *drp;
    struct dt_dentry *d;
    unsigned int pid, id, fid, items;
    unsigned long long size;
    const char *s;
    char *name;

    switch (line[0]) {
        case '0':
            if ((dr = dtread_data_alloc()) == NULL)
                return 0;
            if ((dr->de->id = atoi(&line[2])) == 0 ) {
                LOG_ERR("parsing failed for line %s\n", line);
                dtread_data_free(dr);
                return 0;
            }
            if (!cuckoo_insert(cu, dr->de->id, (void *) dr)) {
                dtread_data_free(dr);
                return 0;
            }
            if (dr->de->id > *maxid)
                *maxid = dr->de->id;
            break;
        case '1':
            
            s = strchr(line, ' ');
            if ((s == NULL) || (sscanf(++s, "%u", &pid) < 1 )) {
                LOG_ERR("parsing pid failed for line %s\n", line);
                return 0;
            }
            
            if (pid != 0) {
                drp = (struct dtread_data *) cuckoo_lookup(cu, pid);
                if (drp == NULL) {
                    LOG_ERR("cuckoo_lookup() returned null for id %u line %s\n",
                        pid, line);
                    return 0;
                }
            }

            s = strchr(s, ' ');
            if ((s == NULL) || (sscanf(++s, "%u", &fid) < 1)) {
                LOG_ERR("Parsing fid failed for line %s\n", line);
                return 0;
            }
            s = strchr(s, ' ');
            if ((s == NULL) || (sscanf(++s, "%llu", &size) < 1)) {
                LOG_ERR("Parsing size failed for line %s\n", line);
                return 0;
            }
            s = strchr(s, ' ');
            if ((s == NULL) || (sscanf(++s, "%u", &id) < 1)) {
                LOG_ERR("Parsing id failed for line %s\n", line);
                return 0;
            }
            s = strchr(s, ' ');
            if ((s == NULL) || (sscanf(++s, "%u", &items) < 1)) {
                LOG_ERR("Parsing items failed for line %s\n", line);
                return 0;
            }
            s = strchr(s, ' ');
            if ((s == NULL) || ((name = strdup(++s))== NULL)) {
                LOG_ERR("Parsing name failed for line %s\n", line);
                return 0;
            }

#if 0
            printf("Parsed line %s\n pid=%u fid=%u size=%llu id=%u items=%u\n",
                line, pid, fid, size, id, items);
#endif

            if (id == 0) {
                /* file */
                if ((d = dt_alloc()) == NULL) {
                    LOG_ERR("dt_alloc() returned NULL\n");
                    free(name);
                    return 0;
                }
                d->type = DT_FILE;
                d->fid = fid;
                d->size = size;
                d->name = name;
                d->parent = drp->de;
                if (drp->file_child)
                    drp->file_child->sibling = d;
                else
                    drp->de->file_child = d;
                drp->file_child = d;
            } else {
                /* directory */
                dr = (struct dtread_data *) cuckoo_lookup(cu, id);
                if (dr == NULL) {
                    LOG_ERR("cuckoo_lookup() returned null for id %u\n", id);
                    return 0;
                }
                d = dr->de; 
                d->type = DT_DIR;
                d->fid = fid;
                d->size = size;
                d->name = name;
                d->items = items;

                if (pid != 0){
                    if (drp->child)
                        drp->child->sibling = d;
                    else
                        drp->de->child = d;
                    drp->child = d;
                    d->parent = drp->de;
                    cuckoo_delete(cu, id);
                    free(dr);
                }
            }

            break;
        case '+':
        case '-':
        case '*':
            return 1;
        default:
            LOG_ERR("Unknown line format: %s\n", line);
            return 0;
    }
    return 1;
}

struct dt_dentry * dtread_readfile(const char *filename, unsigned int *maxid)
{
    FILE *file;
    int c;
    char ch;
    struct buf_str *bs;
    struct cuckoo_ctx *cu;
    struct dtread_data *dr;
    struct dt_dentry *root = NULL;

    LOG_ASSERT(filename != NULL, "Bad arguments\n");

    if ((file = fopen(filename, "r")) == NULL) {
        LOG_ERR("Can't open file %s\n", filename);
        return NULL;
    }

    if ((bs = buf_alloc()) == NULL)
        goto clear_file;
    
    if ((cu = cuckoo_alloc(0)) == NULL)
        goto clear_bs;

    while ((c = fgetc(file)) != EOF) {
        if (c == '\n') {
            if (buf_error(bs))
                goto clear_cu;
            if (!dtread_readline(buf_string(bs), cu, maxid))
                goto clear_cu;
            buf_clear(bs);
        } else {
            ch = c;
            buf_appendn(bs, &ch, 1);
        }
    }

    if (cuckoo_items(cu) != 1) {
        LOG_ERR("Some directories are left in cockoo tables\n");
        goto clear_cu;
    }

    if ((dr = cuckoo_lookup(cu, 1)) == NULL) {
        LOG_ERR("No root node in cuckoo table\n");
        goto clear_cu;
    }
   
    root = dr->de;
    cuckoo_delete(cu, 1);
    free(dr);

clear_cu:
    cuckoo_rfree(cu, dtread_data_free);

clear_bs:
    buf_free(bs);

clear_file:
    fclose(file);

    return root;
}

#if 0
extern void dt_reverse_p(struct dt_dentry *root);

int main(int ac, char **av)
{
    struct dt_dentry *root;

    root = dtread_readfile(av[1]);
    if (root != NULL) {
        dt_reverse_p(root);
        dt_free(root);
    } else
        printf("dtread_readfile() returned NULL\n");

    return 0;
}
#endif

