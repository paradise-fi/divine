/*!\file
 * This file contains the funcions which can convert some integer types to
 * the string representation (e. g. convert 2345 to "2345").
 *
 * It also contains the function dispose_string that only calls free() on a
 * given argument.
 *
 * These function are only used in error.hh (\eunit) for converting
 * numbers on the error input to strings.
 */
#ifndef _INTTOSTR_HH_
#define _INTTOSTR_HH_

#ifndef DOXYGEN_PROCESSING
namespace divine //We want Doxygen not to see namespace `dve'
{
#endif //DOXYGEN_PROCESSING
 char * create_string_from_uint(const unsigned long int i);
 char * create_string_from_int(const signed long int i);
 template<class T> static inline char * create_string_from(const T i);
 
 template <>
 char * create_string_from<unsigned long int>(const unsigned long int i)
 { return create_string_from_uint(i); }
 
 template <>
 char * create_string_from<signed long int>(const signed long int i)
 { return create_string_from_int(i); }

 template <>
 char * create_string_from<unsigned int>(const unsigned int i)
 { return create_string_from_uint(i); }
 
 template <>
 char * create_string_from<signed int>(const signed int i)
 { return create_string_from_int(i); }
 
 void dispose_string(char * const str);

#ifndef DOXYGEN_PROCESSING
}; //end of namespace `dve'
#endif

#endif
