/* bitcollect.c - stretch unsigned numbers collector
 *
 * Copyright 2010, Radist <radist.nt@gmail.com>
 * Read the COPYING file in the root of the source tree.
 */

#include <memory.h>

#include "bitcollect.h"
#include "log.h"

#if (1 << BITVECTOR_ELEMENT_SHIFT) != BITVECTOR_ELEMENT_VOL
#error Invalid defines in bitcollect.h
#endif

#define NO_CACHE (size_t)(-1)
#define _NO_ZERO ((BITVECTOR_EL)(-1))

static size_t bitvector_align_size(size_t req_size)
{
	return (req_size + BITVECTOR_SIZE_STEP - 1) & ~(BITVECTOR_SIZE_STEP - 1);
}

static int bitvector_expand(bitvector *bv, size_t old_size, size_t new_size)
{
	bitvector newvec;
	LOG_ASSERT(bv != NULL && new_size >= old_size, "Bad arguments\n");
	if (new_size == old_size)
		return 1;
	newvec = (bitvector)calloc(new_size + 1, BITVECTOR_ELEMENT_SIZE);
	if (newvec != NULL)
		memcpy(newvec, *bv, old_size);
	else
		LOG_ERRNO("calloc() returned NULL\n");
	free(*bv);
	*bv = newvec;
	return newvec != NULL;
}

static size_t bitvector_segment(size_t index, size_t *offset)
{
	if (offset != NULL)
		*offset = index & (BITVECTOR_ELEMENT_VOL - 1);
	return index >> BITVECTOR_ELEMENT_SHIFT;
}

static BITVECTOR_EL * bitvector_ptr(bitvector bv, size_t index, size_t *offset)
{
	LOG_ASSERT(bv != NULL, "Bad argument\n");
	return bv + bitvector_segment(index, offset);
}

static size_t bitvector_findzero_el(BITVECTOR_EL el)
{
	size_t res = 0;
	while (el & 1) {
		el >>= 1;
		++res;
	}
	return res;
}

static size_t bitvector_findzero(bitvector bv)
{
	BITVECTOR_EL * el = bv;
	LOG_ASSERT(bv != 0, "Bad argument\n");
	while (*el == _NO_ZERO)
		++el;
	return ((el - bv) << BITVECTOR_ELEMENT_SHIFT) + bitvector_findzero_el(*el);
}

static void bitvector_set_bit(bitvector bv, size_t index)
{
	LOG_ASSERT(bv != NULL, "Bad argument\n");
	*bitvector_ptr(bv, index, &index) |= (BITVECTOR_EL)1 << index;
}

static void bitvector_reset_bit(bitvector bv, size_t index)
{
	LOG_ASSERT(bv != NULL, "Bad argument\n");
	*bitvector_ptr(bv, index, &index) &= ~((BITVECTOR_EL)1 << index);
}

static int bc_checksize(struct bit_collect *bc, size_t element)
{
	LOG_ASSERT(bc != NULL, "Bad argument\n");
	element = bitvector_segment(element, NULL);
	if (bc->vec_size <= element) {
		element = bitvector_align_size(element);
		if ( !(
			bitvector_expand(&bc->mvec, bc->vec_size, element) &&
			bitvector_expand(&bc->ivec,
				bitvector_align_size(bitvector_segment(bc->vec_size, NULL)),
				bitvector_align_size(bitvector_segment(element, NULL)))
				) ) {
			bc->cached_segment = NO_CACHE;
			return 0;
		}
		bc->vec_size = element;
	}
	return 1;
}

static void bc_cache_segment(struct bit_collect *bc, size_t segment)
{
	LOG_ASSERT(bc != NULL && bc->vec_size > segment, "Bad argument\n");
	LOG_ASSERT(bc->cached_segment != NO_CACHE, "Cache disabled\n");
	if (segment == bc->cached_segment)
		return;
	bc->mvec[bc->cached_segment] = bc->cached_data;
	bc->cached_data = bc->mvec[bc->cached_segment = segment];
}

static size_t bc_cached_findzero_set(struct bit_collect *bc)
{
	size_t res;
	LOG_ASSERT(bc != NULL, "Bad argument\n");
	LOG_ASSERT(bc->cached_segment != NO_CACHE, "Cache disabled\n");
	res = bitvector_findzero_el(bc->cached_data);
	bc->cached_data |= (BITVECTOR_EL)1 << res;
	return (bc->cached_segment << BITVECTOR_ELEMENT_SHIFT) + res;
}

static int bc_index_findzero(struct bit_collect * bc)
{
	size_t res = bitvector_findzero(bc->ivec);
	if (res < bitvector_segment(bc->min_new, NULL)) { /* found */
		bc_cache_segment(bc, res);
		bitvector_set_bit(bc->ivec, res);
		return 1;
	}
	return 0;
}

void bc_init(struct bit_collect *bc)
{
	LOG_ASSERT(bc != NULL, "Bad argument\n");
	memset(bc, 0, sizeof(struct bit_collect));
	bc->mvec = (bitvector)calloc(BITVECTOR_SIZE_STEP, BITVECTOR_ELEMENT_SIZE);
	bc->ivec = (bitvector)calloc(BITVECTOR_SIZE_STEP, BITVECTOR_ELEMENT_SIZE);
	if (bc->mvec != NULL && bc->ivec != NULL)
		bc->vec_size = BITVECTOR_SIZE_STEP;
	else {
		LOG_ERRNO("calloc() returned NULL\n");
		bc->cached_segment = NO_CACHE;
	}
}

void bc_uninit(struct bit_collect *bc)
{
	LOG_ASSERT(bc != NULL, "Bad argument\n");
	free(bc->mvec);
	free(bc->ivec);
	bc->vec_size = 0;
}

void bc_set(struct bit_collect *bc, size_t number)
{
	LOG_ASSERT(bc != NULL, "Bad argument\n");
	if (number >= bc->min_new) {
		bc->min_new = bitvector_segment(number + BITVECTOR_ELEMENT_VOL, NULL) << BITVECTOR_ELEMENT_SHIFT;
		if (bc->cached_segment == NO_CACHE || !bc_checksize(bc, number))
			return;
	} else if (bc->cached_segment == NO_CACHE)
		return;
	bc_cache_segment(bc, bitvector_segment(number, &number));
	if (0 == bc->cached_data)
		bitvector_set_bit(bc->ivec, bc->cached_segment);
	bc->cached_data |= (BITVECTOR_EL)1 << number;
}

size_t bc_getnew(struct bit_collect *bc)
{
	size_t index;
	LOG_ASSERT(bc != NULL, "Bad argument\n");
	if (bc->cached_segment != NO_CACHE) {
		if (bc->cached_data!=_NO_ZERO || bc_index_findzero(bc))
			return bc_cached_findzero_set(bc);
		bc->mvec[bc->cached_segment] = bc->cached_data; /* flush cache */
		index = 0;
		while ((index = (index << BITVECTOR_ELEMENT_SHIFT) + bitvector_findzero(bc->mvec + index)) < bc->min_new) {
			bitvector_reset_bit(bc->ivec, index >= BITVECTOR_ELEMENT_SHIFT);
			++index;
		}
		if (bc_index_findzero(bc))
			return bc_cached_findzero_set(bc);
		bc->cached_segment = NO_CACHE;
	}
	return bc->min_new++;
}

