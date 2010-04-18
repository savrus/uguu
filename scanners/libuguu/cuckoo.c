/* cuckoo.c - cuckoo hashing
 *
 * Copyright 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "cuckoo.h"
#include "log.h"

static uint32_t cuckoo_index(uint32_t x, uint32_t b, unsigned int log)
{
    return ((b * x) >> (32 - log)) * (log != 0);
}

struct cuckoo_ctx * cuckoo_alloc(unsigned int log)
{
    struct cuckoo_ctx *cu;
    size_t size;
    LOG_ASSERT(log < 30, "Too big desired size\n");

    cu = (struct cuckoo_ctx *) calloc(1, sizeof(struct cuckoo_ctx));
    if (cu == NULL) {
        LOG_ERR("calloc() returned NULL\n");
        return NULL;
    }

    size = 1 << log;

    cu->table1 = (struct cuckoo_tbl *) calloc(size, sizeof(struct cuckoo_tbl));
    cu->table2 = (struct cuckoo_tbl *) calloc(size, sizeof(struct cuckoo_tbl));
    if ((cu->table1 == NULL) || (cu->table2 == NULL)) {
        LOG_ERR("calloc() returned NULL\n");
        free(cu->table1);
        free(cu->table2);
        free(cu);
        return NULL;
    }
    
    /* assume RAND_MAX be 2^31 -1 */
    cu->hash1 = (random() << 1) | 1;
    cu->hash2 = (random() << 1) | 1;
    cu->size = size; 
    cu->log = log;
    return cu;
}

void cuckoo_free(struct cuckoo_ctx *cu)
{
    free(cu->table1);
    free(cu->table2);
    free(cu);
}

void * cuckoo_lookup(struct cuckoo_ctx *cu, uint32_t key)
{
    uint32_t ind;

#if 0
    int i;
    printf("Lookup key %u. cuckoo size: %zu\n", key, cu->size);
    for (i=0; i < cu->size; i++)
        printf("%u ", cu->table1[i].key);
    printf("\n");
    for (i=0; i < cu->size; i++)
        printf("%u ", cu->table2[i].key);
    printf("\n");
#endif 

    ind = cuckoo_index(key, cu->hash1, cu->log);
    if (cu->table1[ind].key == key)
        return cu->table1[ind].data;
    ind = cuckoo_index(key, cu->hash2, cu->log);
    if (cu->table2[ind].key == key)
        return cu->table2[ind].data;
    return NULL;
}

/* return 1 if key is present, 0 if not */
static int cuckoo_present(struct cuckoo_ctx *cu, uint32_t key)
{
    uint32_t ind;

    ind = cuckoo_index(key, cu->hash1, cu->log);
    //printf("present: ind = %d key= %d log=%d\n", ind, key, cu->log);
    if (cu->table1[ind].key == key)
        return 1;
    ind = cuckoo_index(key, cu->hash2, cu->log);
    if (cu->table2[ind].key == key)
        return 1;
    return 0;
}

/* return 0 if failed, 1 if succeeded */
static int cuckoo_move(struct cuckoo_ctx *old, struct cuckoo_ctx *new)
{
    size_t i;

    for (i = 0; i < old->size; i++) {
        if (old->table1[i].key != 0)
            if (!cuckoo_insert(new, old->table1[i].key, old->table1[i].data))
                return 0;
        if (old->table2[i].key != 0)
            if (!cuckoo_insert(new, old->table2[i].key, old->table2[i].data))
                return 0;
    }
    return 1;
}

/* return 0 if failed, 1 if succeeded */
static int cuckoo_rehash(struct cuckoo_ctx *cu, unsigned int log)
{
    struct cuckoo_ctx *cun;

    if ((cun = cuckoo_alloc(log)) == NULL)
        return 0;
    if (!cuckoo_move(cu, cun)) {
        cuckoo_free(cun);
        return 0;
    }
    free(cu->table1);
    free(cu->table2);
    *cu = *cun;
    free(cun);
    return 1;
}

/* return 0 if failed, 1 if succeeded */
int cuckoo_insert(struct cuckoo_ctx *cu, uint32_t key, void *data)
{
    uint32_t ind;
    unsigned int i;
    struct cuckoo_tbl tn = {key, data}, tmp;
    LOG_ASSERT((cu != NULL), "Bad arguments\n");

    if (key == 0) {
        LOG_ERR("Usage of key = 0 is not allowed\n");
        return 0;
    }

    if (cuckoo_present(cu, key)) {
        LOG_ERR("Element with key %u already in hash table\n", key);
        return 0;
    }

    if ((cu->size - 1 <= cu->items) && (!cuckoo_rehash(cu, cu->log + 1)))
        return 0;

    for (i = 0; i < cu->log * 3; i++) {
        ind = cuckoo_index(tn.key, cu->hash1, cu->log);
        if (cu->table1[ind].key == 0) {
            cu->table1[ind] = tn;
            cu->items++;
            return 1;
        }
        tmp = cu->table1[ind];
        cu->table1[ind] = tn;
        tn = tmp;
        ind = cuckoo_index(tn.key, cu->hash2, cu->log);
        if (cu->table2[ind].key == 0) {
            cu->table2[ind] = tn;
            cu->items++;
            return 1;
        }
        tmp = cu->table2[ind];
        cu->table2[ind] = tn;
        tn = tmp;
    }

#if 0
    printf("insert failed: rehash!\n");
#endif
    if (!cuckoo_rehash(cu, cu->log))
        return 0;
    return cuckoo_insert(cu, tn.key, tn.data);
}

/* return 0 if failed, 1 if succeeded */
int cuckoo_delete(struct cuckoo_ctx *cu, uint32_t key)
{
    uint32_t ind;

    ind = cuckoo_index(key, cu->hash1, cu->log);
    if (cu->table1[ind].key == key) {
        cu->items--;
        cu->table1[ind].key = 0;
    }
    ind = cuckoo_index(key, cu->hash2, cu->log);
    if (cu->table2[ind].key == key) {
        cu->items--;
        cu->table2[ind].key = 0;
    }
    
    if (cu->items < (1 << (cu->log - 1)))
        return cuckoo_rehash(cu, cu->log - 1);
    return 1;
}

size_t cuckoo_items(struct cuckoo_ctx *cu)
{
    return cu->items;
}

void cuckoo_rfree(struct cuckoo_ctx *cu, void (*fr) (void*))
{
    size_t i;

    for (i = 0; i < cu->size; i++) {
        if (cu->table1[i].key != 0)
            fr(cu->table1[i].data);
        if (cu->table2[i].key != 0)
            fr(cu->table2[i].data);
    }

    cuckoo_free(cu);
}

