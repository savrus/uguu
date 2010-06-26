/* buf - self-enlarging buffer to store string, wide-character version
 *
 * Copyright 2009, 2010, savrus
 * Copyright 2010, Radist <radist.nt@gmail.com>
 * Read the COPYING file in the root of the source tree.
 */

/* all additional includes should come before buf.h */
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "log.h"

/* force buf.h to include buf.c after the redefines */
#define WBUF_C

#include "wbuf.h"

#undef WBUF_C
