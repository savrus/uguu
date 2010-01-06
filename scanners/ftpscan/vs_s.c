/* vs_s.c - Visual Studio *_s functions support (only required ones)
 *
 * Copyright 2010, Radist <radist.nt@gmail.com>
 * Read the COPYING file in the root of the source tree.
 */

#include "vs_s.h"

errno_t _strupr_s(char * _Str, size_t _Size)
{
	CheckArg(_Str,);
	if(!*_Str) return 0;
	while(_Size) {
		if( *_Str >= 'a' && *_Str <='z' )
			_Str += 'A' - 'a';
		if( !*++_Str ) return 0;
		--_Size;
	}
	return ERANGE;
}

int sprintf_s(char * _DstBuf, size_t _SizeInBytes, const char * _Format, ...)
{
	CheckArg2(_DstBuf && _Format);
	va_list _Args;
	va_start(_Args, _Format);
	int result = vsnprintf(_DstBuf, _SizeInBytes, _Format, _Args);
	va_end(_Args);
	return result;
}
