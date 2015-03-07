// -*- C++ -*- (c) 2015 Jiří Weiser

#ifndef _BITS_TYPES_H
#define _BITS_TYPES_H

#ifndef __clang__
# error "Header file <bits/types.h> must be compiled via clang."
#endif

/* clang specific macros */
typedef __SIZE_TYPE__               __size_t;
typedef __INT8_TYPE__               __int8_t;
typedef __INT16_TYPE__              __int16_t;
typedef __INT32_TYPE__              __int32_t;
typedef __INT64_TYPE__              __int64_t;
typedef __INTPTR_TYPE__             __intptr_t;
typedef __PTRDIFF_TYPE__            __ptrdiff_t;
typedef __INTMAX_TYPE__             __intmax_t;

/* unsigned versions of previous types */
#ifdef __UINT8_TYPE__
typedef __UINT8_TYPE__              __uint8_t;
#else
typedef unsigned __INT8_TYPE__      __uint8_t;
#endif
#ifdef __UINT16_TYPE__
typedef __UINT16_TYPE__             __uint16_t;
#else
typedef unsigned __INT16_TYPE__     __uint16_t;
#endif
#ifdef __UINT32_TYPE__
typedef __UINT32_TYPE__             __uint32_t;
#else
typedef unsigned __INT32_TYPE__     __uint32_t;
#endif
#ifdef __UINT64_TYPE__
typedef __UINT64_TYPE__             __uint64_t;
#else
typedef unsigned __INT64_TYPE__     __uint64_t;
#endif
#ifdef __UINTPTR_TYPE__
typedef __UINTPTR_TYPE__            __uintptr_t;
#else
typedef unsigned __INTPTR_TYPE__    __uintptr_t;
#endif
#ifdef __UINTMAX_TYPE__
typedef __UINTMAX_TYPE__            __uintmax_t;
#else
typedef unsigned __INTMAX_TYPE__    __uintmax_t;
#endif



/* Convenience types.  */
typedef unsigned char       __u_char;
typedef unsigned short int  __u_short;
typedef unsigned int        __u_int;
typedef unsigned long int   __u_long;

#define __CHAR_MIN          (-__SCHAR_MAX__-1)
#define __CHAR_MAX          __SCHAR_MAX__
#define __UCHAR_MAX         (2*__SCHAR_MAX__+1)

#define __SHORT_MIN         (-__SHRT_MAX__-1)
#define __SHORT_MAX         __SHRT_MAX__
#define __USHORT_MAX        (2*__SHRT_MAX__+1)

#define __INT_MIN           (-__INT_MAX__-1)
#define __INT_MAX           __INT_MAX__
#define __UINT_MAX          (2*__INT_MAX__+1)

#define __LONG_MIN          (-__LONG_MAX__-1)
#define __LONG_MAX          __LONG_MAX__
#define __ULONG_MAX         (2*__LONG_MAX__+1)

#define __LONG_LONG_MIN     (-__LONG_LONG_MAX__-1)
#define __LONG_LONG_MAX     __LONG_LONG_MAX__
#define __ULONG_LONG_MAX    (2*__LONG_LONG_MAX__+1)


#define __INT8_MIN          __CHAR_MIN
#define __INT8_MAX          __CHAR_MAX
#define __UINT8_MAX         __UCHAR_MAX

#if __SIZEOF_SHORT__ == 2
# define __INT16_MIN        __SHORT_MIN
# define __INT16_MAX        __SHORT_MAX
# define __UINT16_MAX       __USHORT_MAX
# define __INT16_      2
#elif __SIZEOF_INT__ == 2
# define __INT16_MIN        __INT_MIN
# define __INT16_MAX        __INT_MAX
# define __UINT16_MAX       __UINT_MAX
#else
# error Cannot get limit values for int16_t
#endif

#if __SIZEOF_SHORT__ == 4
# define __INT32_MIN        __SHORT_MIN
# define __INT32_MAX        __SHORT_MAX
# define __UINT32_MAX       __USHORT_MAX
#elif __SIZEOF_INT__ == 4
# define __INT32_MIN        __INT_MIN
# define __INT32_MAX        __INT_MAX
# define __UINT32_MAX       __UINT_MAX
#elif __SIZEOF_LONG__ == 4
# define __INT32_MIN        __LONG_MIN
# define __INT32_MAX        __LONG_MAX
# define __UINT32_MAX       __ULONG_MAX
#else
# error Cannot get limit values for int32_t
#endif

#if __SIZEOF_INT__ == 8
# define __INT64_MIN        __INT_MIN
# define __INT64_MAX        __INT_MAX
# define __UINT64_MAX       __UINT_MAX
#elif __SIZEOF_LONG__ == 8
# define __INT64_MIN        __LONG_MIN
# define __INT64_MAX        __LONG_MAX
# define __UINT64_MAX       __LONG_MAX
#elif __SIZEOF_LONG_LONG__ == 8
# define __INT64_MIN        __LONG_LONG_MIN
# define __INT64_MAX        __LONG_LONG_MAX
# define __UINT64_MAX       __LONG_LONG_MAX
#else
# error Cannot get limit values for int64_t
#endif


#define __INTPTR_MIN        (-__INTMAX_MAX__-1)
#define __INTPTR_MAX        __INTMAX_MAX__
#define __UINTPTR_MAX       (2*__INTMAX_MAX__+1)

#define __INTMAX_MIN        (-__INTMAX_MAX__-1)
#define __INTMAX_MAX        __INTMAX_MAX__
#define __UINTMAX_MAX       (2*__INTMAX_MAX__+1)

#define __PTRDIFF_MIN       (-__INTMAX_MAX__-1)
#define __PTRDIFF_MAX       __INTMAX_MAX__

#define __SIZE_MAX          __SIZE_MAX__

#define __SIZEOF_SHORT      __SIZEOF_SHORT__
#define __SIZEOF_INT        __SIZEOF_INT__
#define __SIZEOF_LONG       __SIZEOF_LONG__
#define __SIZEOF_LONG_LONG  __SIZEOF_LONG_LONG__

#endif
