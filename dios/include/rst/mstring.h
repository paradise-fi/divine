#pragma once

#include <rst/split.h>

typedef abstract::mstring::Split __mstring;

#define DOMAIN_NAME mstring
#define DOMAIN_KIND content
#define DOMAIN_TYPE __mstring *
    #include <rst/string-domain.def>
#undef DOMAIN_NAME
#undef DOMAIN_TYPE
#undef DOMAIN_KIND

extern "C" {
    __mstring * __mstring_undef_value();

    __mstring * __mstring_realloc( __mstring * str, size_t size );

    __mstring * __mstring_memcpy( __mstring * dst, __mstring * src, size_t size );

    __mstring * __mstring_memmove( __mstring * dst, __mstring * src, size_t size );

    __mstring * __mstring_memset( __mstring * dst, char value, size_t size );
}
