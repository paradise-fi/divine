#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <rst/domains.h>
#include <rst/common.h>
#include <rst/lart.h>

#include <string_view>
#include <util/array.hpp>

namespace abstract::mstring {
	using Segment = std::string_view;

	struct Split {
		using Segments = __dios::Array< Segment >;
		using SubStrs  = __dios::Array< Segments >;

		SubStrs substrs;
	};


    template< typename Buffer >
    struct Quintuple {
        Quintuple( const char * _buff, unsigned _len )
            : buff(_buff, _len)
        {
            for (int i = 0; i < buff.size(); ++i) {
                if ( buff[i] == '\0' )
                    terminators.push_back(i);
            }
            assert(!terminators.empty());
            assert(terminators.front() <= buff.size());
        }

        char * rho() const;
        Split split() const;

        size_t strlen() const;
		int strcmp( const Quintuple< Buffer > * other ) const;

        Buffer buff;			            // IV - buffer
        __dios::Array< int > terminators;   // T - zeros in sbuffer
    };

} // namespace abstract::mstring

using Buffer = std::string_view;

typedef abstract::mstring::Quintuple< Buffer > __mstring;

#define DOMAIN_NAME mstring
#define DOMAIN_KIND string
#define DOMAIN_TYPE __mstring *
    #include <rst/string-domain.def>
#undef DOMAIN_NAME
#undef DOMAIN_TYPE
#undef DOMAIN_KIND
