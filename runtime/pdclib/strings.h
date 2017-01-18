#ifndef _PDCLIB_STRINGS_H
#define _PDCLIB_STRINGS_H _PDCLIB_STRINGS_H
#include <_PDCLIB_int.h>

/* We don't need and should not read this file if <string.h> was already read. */
#ifndef _PDCLIB_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The strcasecmp() function shall compare, while ignoring differences in case,
   the string pointed to by s1 to the string pointed to by s2.

   Upon completion, strcasecmp() shall return an integer greater than, equal to,
   or less than 0, if the string pointed to by s1 is, ignoring case, greater than,
   equal to, or less than the string pointed to by s2, respectively.
 */
int strcasecmp( const char *s1, const char *s2 ) _PDCLIB_nothrow;

/* The strncasecmp() function is similar to strcasecmp, except it only compares
   the first n bytes of s1.

   Upon completion, strncasecmp() shall return an integer greater than, equal to,
   or less than 0, if the string pointed to by s1 is, ignoring case, greater than,
   equal to, or less than the string pointed to by s2, respectively.
 */
int strncasecmp( const char *s1, const char *s2, size_t n ) _PDCLIB_nothrow;

#ifdef __cplusplus
}
#endif

#endif	/* string.h  */

#endif	/* strings.h  */
