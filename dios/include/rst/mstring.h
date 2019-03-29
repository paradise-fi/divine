#pragma once

#include <rst/segmentation.h>

typedef abstract::mstring::sym::Split __mstring;

#define DOMAIN_NAME mstring
#define DOMAIN_KIND content
#define DOMAIN_TYPE __mstring *
    #include <rst/string-domain.def>
#undef DOMAIN_NAME
#undef DOMAIN_TYPE
#undef DOMAIN_KIND

extern "C" {
    __mstring * __mstring_realloc( __mstring * str, size_t size );

    __mstring * __mstring_memcpy( __mstring * dst, __mstring * src, size_t size );

    __mstring * __mstring_memmove( __mstring * dst, __mstring * src, size_t size );

    __mstring * __mstring_memset( __mstring * dst, char value, size_t size );

    __mstring * __mstring_memchr( __mstring * str, int c, size_t n );

    size_t __mstring_fread( __mstring * ptr, size_t size, size_t nmemb, FILE * stream );
    size_t __mstring_fwrite( __mstring * ptr, size_t size, size_t nmemb, FILE * stream );
}
