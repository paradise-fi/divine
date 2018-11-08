#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <rst/domains.h>
#include <rst/common.h>
#include <rst/lart.h>


namespace abstract::mstring {
    struct Quintuple {
        Quintuple()
            : str( nullptr ), len( 0 )
        {}

        Quintuple( const char * str )
            : str( str ), len( strlen( str ) )
        {}

        const char * str;
        size_t len;
    };
} // namespace abstract::mstring

typedef abstract::mstring::Quintuple __mstring;

#define DOMAIN_NAME mstring
#define DOMAIN_KIND string
#define DOMAIN_TYPE __mstring *
    #include <rst/string-domain.def>
#undef DOMAIN_NAME
#undef DOMAIN_TYPE
#undef DOMAIN_KIND
