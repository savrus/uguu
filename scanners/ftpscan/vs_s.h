/* vs_s.h - Visual Studio *_s functions support (only required ones)
 *
 * Copyright 2010, Radist <radist.nt@gmail.com>
 * Read the COPYING file in the root of the source tree.
 */

#ifndef WIN_VS__S_SUPPORT_H__
#define WIN_VS__S_SUPPORT_H__

#ifdef _MSC_VER
#error not required for cl
#endif

#ifndef _TRUNCATE
#define _TRUNCATE ((size_t)-1)
#endif

#ifndef min
#define min(x,y) ((x)>(y)?(y):(x))
#endif

#define CheckArg(x, to_do) \
if( !(x) ) \
	do { \
		to_do; \
		return errno = EINVAL; \
	} while(0)
#define CheckArg2(x) \
if( !(x) ) \
	do { \
		errno = EINVAL; \
		return -1; \
	} while(0)

#ifdef  __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

typedef int errno_t;

static inline errno_t gmtime_s(struct tm * _Tm, const time_t * _Time)
{
	CheckArg(_Tm,);
	CheckArg(_Time && *_Time>0, memset((void*)_Tm, -1, sizeof(struct tm)));
	gmtime_r(_Time, _Tm);
	return 0;
}

errno_t _strupr_s(char * _Str, size_t _Size);
int sprintf_s(char * _DstBuf, size_t _SizeInBytes, const char * _Format, ...);

static inline int _vsnprintf_s(char * _DstBuf, size_t _SizeInBytes, size_t _MaxCount, const char * _Format, va_list _ArgList)
{
	CheckArg2(_DstBuf && _Format && _MaxCount>0);
	int result = vsnprintf(_DstBuf, min(_SizeInBytes, _MaxCount), _Format, _ArgList);
	if( result == -1 ) {
		if(_TRUNCATE == _MaxCount) _MaxCount = _SizeInBytes - 1;
		if(_MaxCount < _SizeInBytes) _DstBuf[_MaxCount] = '\0';
		else *_DstBuf = '\0';
	}
	return result;
}

static inline errno_t memmove_s(void * _Dst, size_t _DstSize, const void * _Src, size_t _MaxCount)
{
	CheckArg(_Dst && _Src,);
	if( _MaxCount>_DstSize ) {
		return errno = ERANGE;
	}
	memmove(_Dst, _Src, _MaxCount);
	return 0;
}

#ifdef  __cplusplus
} // extern "C"

extern "C++" {

template <size_t _Size>
static inline errno_t _strupr_s(char (&_Str)[_Size])
{
	return _strupr_s(_Str, _Size);
}

template <size_t _Size>
static inline int sprintf_s(char (&_DstBuf)[_Size], const char *_Format, ...)
{
	CheckArg2(_DstBuf && _Format);
	va_list _Args;
	va_start(_Args, _Format);
	int result = vsnprintf(_DstBuf, _Size, _Format, _Args);
	va_end(_Args);
	return result;
}

template <size_t _Size>
static inline int _vsnprintf_s(char (&_DstBuf)[_Size], size_t _MaxCount, const char * _Format, va_list _ArgList)
{
	return _vsnprintf_s(_DstBuf, _Size, _MaxCount, _Format, _ArgList);
}

} // extern "C++"
#endif//__cplusplus

#endif//WIN_VS__S_SUPPORT_H__
