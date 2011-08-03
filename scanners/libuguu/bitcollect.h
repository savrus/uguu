/* bitcollect.h - definitions and interfaces of stretch unsigned numbers collector
 *
 * Copyright 2010, Radist <radist.nt@gmail.com>
 * Read the COPYING file in the root of the source tree.
 */

#ifndef BITCOLLECT_H
#define BITCOLLECT_H

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BITVECTOR_SIZE_STEP 64
#define BITVECTOR_EL uint32_t
#define BITVECTOR_ELEMENT_SHIFT 5
#define BITVECTOR_ELEMENT_SIZE 4 /* sizeof(BITVECTOR_EL) */
#define BITVECTOR_ELEMENT_VOL (8*BITVECTOR_ELEMENT_SIZE)

typedef BITVECTOR_EL * bitvector;

// bit collector structure
struct bit_collect {
    bitvector mvec;
    bitvector ivec;
    size_t min_new;
    size_t vec_size;
    BITVECTOR_EL cached_data;
    size_t cached_segment;
};

// init and uninit bit collector structure routines
void bc_init(struct bit_collect *bc);
void bc_fini(struct bit_collect *bc);

// add a number to the bit collector structure
void bc_set(struct bit_collect *bc, size_t number);
// choose any not added before number and add it to the bit collector structure
size_t bc_getnew(struct bit_collect *bc);

#ifdef __cplusplus
}
#endif

#endif /* BITCOLLECT_H */
