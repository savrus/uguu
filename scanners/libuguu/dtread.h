/* dtread - definitions and interfaces of tree reading
 *
 * Copyright 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#ifndef DTREAD_H
#define DTREAD_H

#include <stdio.h>

#include "dt.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dtread_data {
    struct dt_dentry *de;
    struct dt_dentry *child;
    struct dt_dentry *file_child;
};

/* reconstruct tree from file. Returns NULL on failure */
struct dt_dentry * dtread_readfile(FILE *file, unsigned int *maxid);

#ifdef __cplusplus
}
#endif

#endif /* DTREAD_H */

