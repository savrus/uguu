/* wbuf - definitions and interfaces of self-enlarging buffer, wide-character version
 *
 * Copyright 2009, 2010, savrus
 * Copyright 2010, Radist <radist.nt@gmail.com>
 * Read the COPYING file in the root of the source tree.
 */

#ifndef WBUF_H
#define WBUF_H

/* this includes should come before all redefines */
#include <stddef.h>
#include <stdarg.h>

/* types */
#define char            wchar_t
#define buf_str         wbuf_str
/* library routines */
#define strlen          wcslen
#define strcpy          wcscpy
#define strncpy         wcsncpy
#define vsnprintf       _vsnwprintf
/* routines */
#define buf_alloc       wbuf_alloc
#define buf_chop        wbuf_chop
#define buf_clear       wbuf_clear
#define buf_expand      wbuf_expand
#define buf_append      wbuf_append
#define buf_appendn     wbuf_appendn
#define buf_appendf     wbuf_appendf
#define buf_vappendf    wbuf_vappendf
#define buf_string      wbuf_string
#define buf_strlen      wbuf_strlen
#define buf_error       wbuf_error
#define buf_free        wbuf_free

/* BUF_H macro workaround */
#ifdef BUF_H
#define WBUF_BUF_H
#undef BUF_H
#endif

#ifdef WBUF_C
#include "buf.c"
#else
#include "buf.h"
#endif

/* BUF_H macro workaround */
#ifdef WBUF_BUF_H
#undef WBUF_BUF_H
#else
#undef BUF_H
#endif

/* types */
#undef char
#undef buf_str
/* library routines */
#undef strlen
#undef strcpy
#undef strncpy
#undef vsnprintf
/* routines */
#undef buf_alloc
#undef buf_chop
#undef buf_clear
#undef buf_expand
#undef buf_append
#undef buf_appendn
#undef buf_appendf
#undef buf_vappendf
#undef buf_string
#undef buf_strlen
#undef buf_error
#undef buf_free

#endif /* WBUF_H */
