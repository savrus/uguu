/* stdint.h - integer types missing in VS
 *
 * Copyright 2010, Radist <radist.nt@gmail.com>
 * Read the COPYING file in the root of the source tree.
 */

#ifndef STDINT_H
#define STDINT_H

#ifdef _MSC_VER

typedef signed   __int8    int8_t;
typedef signed   __int16  int16_t;
typedef signed   __int32  int32_t;
typedef signed   __int64  int64_t;

typedef unsigned __int8   uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;


#define __iMIN(n) ((int##n##_t)((uint##n##_t)1 << (n-1)))
#define __iMAX(n) ((int##n##_t)((uint##n##_t)1 << (n-1) - 1))
#define __uMAX(n) ((uint##n##_t)(-1ll))

#define   INT8_MIN __iMIN(8)
#define   INT8_MAX __iMAX(8)
#define  UINT8_MAX __uMAX(8)
#define  INT16_MIN __iMIN(16)
#define  INT16_MAX __iMAX(16)
#define UINT16_MAX __uMAX(16)
#define  INT32_MIN __iMIN(32)
#define  INT32_MAX __iMAX(32)
#define UINT32_MAX __uMAX(32)
#define  INT64_MIN __iMIN(64)
#define  INT64_MAX __iMAX(64)
#define UINT64_MAX __uMAX(64)

#else  /* _MSC_VER */
#error Only for VS
#endif /* _MSC_VER */

#endif /* STDINT_H */
