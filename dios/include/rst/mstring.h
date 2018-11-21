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
            : index(0), end(0), str( nullptr )
        {}

        Quintuple( const char * buff, unsigned buff_len )
            : index(::strlen(buff)), end(buff_len), str(buff)
        {}

        char * rho() const;

        size_t strlen() const;

        size_t index;                        // IV - index of first \0
	    static constexpr unsigned begin = 0; // lower_bound - begin of buffer
	    unsigned end;                        // upper_bound - end of buffer
        const char * str;                    // T - buffer
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
