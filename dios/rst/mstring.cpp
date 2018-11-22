#include <rst/mstring.h>

#include <rst/common.h>

#include <algorithm> // min

using namespace abstract::mstring;
using abstract::mark;
using abstract::__new;

__mstring * __mstring_lift( const char * buff, unsigned buff_len ) {
    return __new< Quintuple< Buffer > >( buff, buff_len );
}

extern "C" {
    _MSTRING const char * __mstring_val( const char * buff, unsigned buff_len ) {
        auto val = __mstring_lift( buff, buff_len ); // TODO copy str
        __lart_stash( reinterpret_cast< uintptr_t >( val ) );
        return abstract::__taint< const char * >( buff );
    }

    size_t __mstring_strlen( const __mstring * str ) {
        return str->strlen();
    }

    int __mstring_strcmp( const __mstring * lhs, const __mstring * rhs ) {
        return lhs->strcmp( rhs );
    }
}

template< typename Buffer >
char * Quintuple< Buffer >::rho() const {
    return nullptr;
}

template< typename Buffer >
size_t Quintuple< Buffer >::strlen() const {
    if ( buff.data() == nullptr )
        return -1;
    assert(!terminators.empty());
    return terminators.front();
}

template< typename Buffer >
Split::Segments segments( const Buffer& buff ) {
    Split::Segments segs;

	auto str = buff;
    while ( !buff.empty() ) {
        auto segment_end = str.find_first_not_of( buff.front() );
		segs.push_back( buff.substr( 0, segment_end ) );
        str.remove_prefix( std::min( segment_end, str.size() ) );
    }

    return segs;
}


template< typename Buffer >
Split Quintuple< Buffer >::split() const {
    Split split;

    if ( buff.data() != nullptr ) {
        assert(!terminators.empty());

        unsigned preffix = 0;
        auto str = buff.substr();
		for ( auto terminator : terminators ) {
			unsigned len = terminator - preffix;
			split.substrs.push_back( segments( str.substr( 0, len ) ) );
			str.remove_prefix( len + 1 );
			preffix = terminator + 1;
        }

		// last substring
		split.substrs.push_back( segments( str ) );
    }

	return split;
}

template< typename Buffer >
int Quintuple< Buffer >::strcmp( const Quintuple *other ) const {
	auto split_lhs = split();
	auto split_rhs = other->split();

	const auto& segs_lhs = split_lhs.substrs.front();
	const auto& segs_rhs = split_rhs.substrs.front();

	assert((!segs_lhs.empty() || !segs_rhs.empty()) && "Error: there is no string of interest.");
	return buff.substr( 0, terminators[0] ).compare( other->buff.substr( 0, other->terminators[0] ) );
}
