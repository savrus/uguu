/* cuckoo.h -  definitions and interfaces of cuckoo hashing
 *
 * Copyright 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#ifndef CUCKOO_H
#define CUCKOO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cuckoo_tbl {
    uint32_t key;
    void *data;
};

struct cuckoo_ctx {
    struct cuckoo_tbl *table1;
    struct cuckoo_tbl *table2;
    uint32_t hash1;
    uint32_t hash2;
    size_t size;
    size_t items;
    unsigned int log;
};

/* allocate cuckoo context. log is a logarithm base 2 of initial size */
struct cuckoo_ctx * cuckoo_alloc(unsigned int log);

/* free context */
void cuckoo_free(struct cuckoo_ctx *cu);

/* get data associated with a key */
void * cuckoo_lookup(struct cuckoo_ctx *cu, uint32_t key);

/* insert a pair of key,data into hash. returns 0 on failure, 1 otherwise */
int cuckoo_insert(struct cuckoo_ctx *cu, uint32_t key, void *data);

/* delete key from hash. returns 0 on failure, 1 otherwise */
int cuckoo_delete(struct cuckoo_ctx *cu, uint32_t key);

/* number of items in hash table */
size_t cuckoo_items(struct cuckoo_ctx *cu);

/* free context with all data stored in hash tables */
void cuckoo_rfree(struct cuckoo_ctx *cu, void (*fr) (void*));

#ifdef __cplusplus
}
#endif

#endif /* CUCKOO_H */

