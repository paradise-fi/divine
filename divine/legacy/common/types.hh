#ifndef DIVINE_TYPES_HH
#define DIVINE_TYPES_HH

#include <cstddef>
#include <stdint.h>


/* TYPES WITH A FIXED SIZE */

namespace divine {

 //!64-bit unsigned integer
 typedef uint64_t ulong_long_int_t;
 //!64-bit signed integer
 typedef int64_t slong_long_int_t;
 //!32-bit unsigned integer
 typedef uint32_t ulong_int_t;
 //!32-bit signed integer
 typedef int32_t slong_int_t;
 //!16-bit unsigned integer
 typedef uint16_t ushort_int_t;
 //!16-bit signed integer
 typedef int16_t sshort_int_t;
 //!8-bit unsigned interger (name without 'u' because of the frequency of usage
 //! of this type)
 typedef uint8_t byte_t;
 //!8-bit signed integer
 typedef uint8_t ubyte_t;
 //!8-bit signed integer
 typedef int8_t sbyte_t;
 //!System dependent integer value defined usually as std::size_t
 //! It should correspond to the maximal addressable space on the computer.
 /*!\warning It can be the potential source of incompatibilities between
  * 64-bit and 32-bit architectures.
  */
 typedef std::size_t size_int_t;

#ifndef __INT64_C
# if __WORDSIZE == 64
#  define __INT64_C(c)  c ## L
# else
#  define __INT64_C(c)  c ## LL
# endif
#endif

#ifndef __UINT64_C
# if __WORDSIZE == 64
#  define __UINT64_C(c) c ## UL
# else
#  define __UINT64_C(c) c ## ULL
# endif
#endif

#ifndef INT8_MIN
# define INT8_MIN               (-128)
#endif
#ifndef INT16_MIN
# define INT16_MIN              (-32767-1)
#endif
#ifndef INT32_MIN
# define INT32_MIN              (-2147483647-1)
#endif
#ifndef INT64_MIN
# define INT64_MIN              (-__INT64_C(9223372036854775807)-1)
#endif
/* Maximum of signed integral types.  */
#ifndef INT8_MAX
# define INT8_MAX               (127)
#endif
#ifndef INT16_MAX
# define INT16_MAX              (32767)
#endif
#ifndef INT32_MAX
# define INT32_MAX              (2147483647)
#endif
#ifndef INT64_MAX
# define INT64_MAX              (__INT64_C(9223372036854775807))
#endif

/* Maximum of unsigned integral types.  */
#ifndef UINT8_MAX
# define UINT8_MAX              (255)
#endif
#ifndef UINT16_MAX
# define UINT16_MAX             (65535)
#endif
#ifndef UINT32_MAX
# define UINT32_MAX             (4294967295U)
#endif
#ifndef UINT64_MAX
# define UINT64_MAX             (__UINT64_C(18446744073709551615))
#endif

#ifndef SIZE_MAX
# if __WORDSIZE == 64
#  define SIZE_MAX              (18446744073709551615UL)
# else
#  define SIZE_MAX              (4294967295U)
# endif
#endif

 //!Number of bytes of ulong_long_int_t and slong_long_int_t
 const int LONG_LONG_INT_BYTES = sizeof(ulong_long_int_t);
 //!Number of bytes of ulong_int_t and slong_int_t
 const int LONG_INT_BYTES = sizeof(ulong_int_t);
 //!Number of bytes of ushort_int_t and sshort_int_t
 const int SHORT_INT_BYTES = sizeof(ushort_int_t);
 //!!Number of bytes of byte_t, ubyte_t and sbyte_t
 const int BYTE_BYTES = sizeof(ubyte_t);
 
 //!Number of bits of ulong_long_int_t and slong_long_int_t
 const int LONG_LONG_INT_BITS = 8*LONG_LONG_INT_BYTES;
 //!Number of bits of ulong_int_t and slong_int_t
 const int LONG_INT_BITS = 8*LONG_INT_BYTES;
 //!Number of bits of ushort_int_t and sshort_int_t
 const int SHORT_INT_BITS  = 8*SHORT_INT_BYTES;
 //!Number of bits of byte_t, ubyte_t and sbyte_t
 const int BYTE_BITS = 8*BYTE_BYTES;
 
 //!Maximal possible value in ulong_int_t
 const ulong_long_int_t MAX_ULONG_LONG_INT = UINT64_MAX;
 //!Minimal possible value in ulong_int_t
 const ulong_long_int_t MIN_ULONG_LONG_INT = ulong_long_int_t(0);
 //!Maximal possible value in slong_int_t
 const slong_long_int_t MAX_SLONG_LONG_INT = INT64_MAX;
 //!Minimal possible value in slong_int_t
 const slong_long_int_t MIN_SLONG_LONG_INT = INT64_MIN;
 //!Maximal possible value in ulong_int_t
 const ulong_int_t MAX_ULONG_INT = UINT32_MAX;
 //!Minimal possible value in ulong_int_t
 const ulong_int_t MIN_ULONG_INT = ulong_int_t(0);
 //!Maximal possible value in slong_int_t
 const slong_int_t MAX_SLONG_INT = INT32_MAX;
 //!Minimal possible value in slong_int_t
 const slong_int_t MIN_SLONG_INT = INT32_MIN;
 //!Maximal possible value in ushort_int_t
 const ushort_int_t MAX_USHORT_INT = UINT16_MAX;
 //!Minimal possible value in ushort_int_t
 const ushort_int_t MIN_USHORT_INT = ushort_int_t(0);
 //!Maximal possible value in sshort_int_t
 const sshort_int_t MAX_SSHORT_INT = INT16_MAX;
 //!Minimal possible value in sshort_int_t
 const sshort_int_t MIN_SSHORT_INT = INT16_MIN;
 //!Maximal possible value in byte_t
 const byte_t MAX_BYTE = UINT8_MAX;
 //!Minimal possible value in byte_t
 const byte_t MIN_BYTE = 0;
 //!Maximal possible value in ubyte_t
 const ubyte_t MAX_UBYTE = UINT8_MAX;
 //!Minimal possible value in ubyte_t
 const ubyte_t MIN_UBYTE = 0;
 //!Maximal possible value in sbyte_t
 const sbyte_t MAX_SBYTE = INT8_MAX;
 //!Minimal possible value in sbyte_t
 const sbyte_t MIN_SBYTE = INT8_MIN;
 //!Maximal possible value in size_int_t (may differ in 32-bit and 64-bit systems!)
 const size_int_t MAX_SIZE_INT = SIZE_MAX;
 //!Minimal possible value in size_int_t
 const size_int_t MIN_SIZE_INT = 0;

 //!Standard constant used in DiVinE for undefined indentifiers.
 const size_int_t NO_ID = MAX_SIZE_INT;
 
 
 //const size_int_t MAX_SIZE_T = 4294967295U;

 //const int DVE_ULONG_INT_BITS = 32;  //number of bits in DVE_ulong_int
 //const int DVE_LONG_INT_BITS  = 32;
 //const int DVE_USHORT_INT_BITS = 16;
 //const int DVE_SHORT_INT_BITS  = 16;
 //const int DVE_BYTE_BITS = 8;
 ///* BOUNDS of some types */
 //const unsigned long int DVE_MAX_ULONG_INT = 4294967295U;
 //const unsigned long int DVE_MIN_ULONG_INT = 0;
 //const signed long int DVE_MAX_LONG_INT =  2147483647;
 //const signed long int DVE_MIN_LONG_INT = (-DVE_MAX_LONG_INT-1);
 //const unsigned short int DVE_MAX_USHORT_INT = 65535u;
 //const unsigned short int DVE_MIN_USHORT_INT = 0;
 //const signed short int DVE_MAX_SHORT_INT =  32767;
 //const signed short int DVE_MIN_SHORT_INT = (-DVE_MAX_SHORT_INT-1);
 //const unsigned short int DVE_MAX_BYTE = 255;
 //const unsigned short int DVE_MIN_BYTE = 0;
 //const size_int_t DVE_MAX_SIZE_T = 4294967295U;

 //const size_int_t DVE_NO_ID = DVE_MAX_SIZE_T;

////it is absolutelly neccessary for these types to be 4,4,2,2 and 1 bytes long!!!
 //typedef ulong_int_t DVE_ulong_int_t;
 //typedef slong_int_t DVE_long_int_t;
 //typedef ushort_int_t DVE_ushort_int_t;
 //typedef sshort_int_t DVE_short_int_t;
 //typedef ubyte_t DVE_byte_t;
/* TYPE USED FOR STORING ARBITRARY VALUE OF ANY VARIABLE IN A DVE SYSTEM */ 
 typedef slong_int_t all_values_t;

}

#endif
