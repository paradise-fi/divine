#include <rst/mstring.h>

#include <rst/common.h>

#include <algorithm> // min

using namespace abstract::mstring;
using abstract::mark;
using abstract::__new;

__mstring * __mstring_lift( const char * buff, unsigned buff_len ) {
    return __new< Quintuple >( buff, buff_len, 0 );
}

extern "C" {
    _MSTRING char * __mstring_val( const char * buff, unsigned buff_len ) {
        auto val = __mstring_lift( buff, buff_len ); // TODO copy str
        __lart_stash( reinterpret_cast< uintptr_t >( val ) );
        return abstract::__taint< char * >( val->data() );
    }

    size_t __mstring_strlen( const __mstring * str ) {
        return str->strlen();
    }

    int __mstring_strcmp( const __mstring * lhs, const __mstring * rhs ) {
        return lhs->strcmp( rhs );
    }

    _MSTRING __mstring * __mstring_undef_value() {
        _UNREACHABLE_F( "Invalid use of mstring value." );
        return nullptr;
    }
}

char * Quintuple::rho() const {
    return nullptr;
}

size_t Quintuple::strlen() const noexcept {
    if ( _buff.data() == nullptr )
        return -1;
    assert(!_terminators.empty());
    return _terminators.front();
}

auto Quintuple::split() const noexcept -> Split< Quintuple::Buffer > {
    if ( _buff.data() != nullptr ) {
        assert(!_terminators.empty());
        return Split( _buff, _terminators );
    }
    _UNREACHABLE_F( "Error: Trying to split buffer without a string of interest." );
}

int Quintuple::strcmp( const Quintuple * other ) const noexcept {
    auto split_q1 = this->split();
    auto split_q2 = other->split();

	auto sq1 =  split_q1.sections().front();
	auto sq2 =  split_q2.sections().front();

	if ( !sq1.empty() && !sq2.empty() ) {
		for ( size_t i = 0; i < sq1.size(); i++ ) {
		    const auto& sq1_seg = sq1.segment_of( i ); // TODO optimize segment_of
		    const auto& sq2_seg = sq2.segment_of( i );

           // TODO optimize per segment comparison
		    if ( sq1_seg.value() == sq2_seg.value() ) {
			    if ( sq1_seg.to() - this->from() > sq2_seg.to() - other->from() ) {
				    return sq1_seg.value() - sq2.segment_of( i + 1 ).value();
			    } else if (sq1_seg.to() - this->from() < sq2_seg.to() - other->from() ) {
				    return sq1.segment_of( i + 1 ).value() - sq2_seg.value();
			    }
		    } else {
				return sq1[ i ] - sq2[ i ];
			}
		}

		return 0;
	}

    _UNREACHABLE_F( "Error: there is no string of interest." );
}

void Quintuple::set( uint64_t idx, char val ) noexcept {
    auto split = this->split();
    auto sq = split.sections().front();

    if ( !sq.empty() ) {
        assert( idx >= _buff.from() && idx < _buff.from() + _buff.size() );
        if ( _buff[ idx ] == '\0' ) {
            auto it = std::lower_bound( _terminators.begin(), _terminators.end(), idx );
            _terminators.erase( it );
        }
        _buff[ idx ] = val;
        if ( val == '\0' ) {
            auto it = std::lower_bound( _terminators.begin(), _terminators.end(), idx );
            _terminators.insert( it, idx );
        }
    } else {
        _UNREACHABLE_F( "Error: there is no string of interest." );
    }
}
