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
            : index(0), lower_bound(0), upper_bound(0), str( nullptr )
        {}

        Quintuple( const char * str )
            : index(0), lower_bound(0), upper_bound(::strlen(str)), str( str )
        {}

        char * rho() const;

        size_t strlen() const;

        size_t index;       // IV
	    int lower_bound;
	    int upper_bound;
        const char * str;   // T
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
