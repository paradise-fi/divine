#include <rst/mstring.h>

#include <rst/common.h>

#include <algorithm> // min

namespace abstract::mstring {

using abstract::__new;

__mstring * __mstring_lift( const char * buff, unsigned buff_len ) {
    return __new< Quintuple >( _VM_PT_Marked, buff, buff_len, 0, 1 );
}

extern "C" {
    _MSTRING char * __mstring_val( char * buff, unsigned buff_len ) {
        auto val = __mstring_lift( buff, buff_len );

        __lart_stash( reinterpret_cast< uintptr_t >( val ) );
        return abstract::taint< char * >( buff );
    }

    bool __mstring_store( char val, __mstring * str, uint64_t idx) {
        str->set( idx, val );
        return true;
    }

    /* String manipulation */
    __mstring * __mstring_strcpy( __mstring * dest, const __mstring * src ) {
        dest->strcpy( src );
        return dest;
    }

    __mstring * __mstring_strncpy( __mstring * /* dest */, const __mstring * /* src */, size_t  /* count */ ) {
        _UNREACHABLE_F( "Not implemented." );
    }

    __mstring * __mstring_strcat( __mstring * dest, const __mstring * src ) {
        dest->strcat( src );
        return dest;
    }

    __mstring * __mstring_strncat( __mstring * /* dest */, const __mstring * /* src */, size_t  /* count */ ) {
        _UNREACHABLE_F( "Not implemented." );
    }

    size_t __mstring_strxfrm( __mstring * /* dest */, const __mstring * /* src */, size_t  /* count */ ) {
        _UNREACHABLE_F( "Not implemented." );
    }

    /* String examination */
    size_t __mstring_strlen( const __mstring * str ) {
        return str->strlen();
    }

    int __mstring_strcmp( const __mstring * lhs, const __mstring * rhs ) {
        return lhs->strcmp( rhs );
    }

    int __mstring_strncmp( const __mstring * /* lhs */, const __mstring * /* rhs */, size_t  /* count */ ) {
        _UNREACHABLE_F( "Not implemented." );
    }

    int __mstring_strcoll( const __mstring * /* lhs */, const __mstring * /* rhs */ ) {
        _UNREACHABLE_F( "Not implemented." );
    }

    __mstring * __mstring_strchr( const __mstring * str, int ch ) {
        return str->strchr( ch );
    }

    __mstring * __mstring_strrchr( const __mstring * /* str */, int /* ch */ ) {
        _UNREACHABLE_F( "Not implemented." );
    }

    size_t __mstring_strspn( const __mstring * /* dest */, const __mstring * /* src */ ) {
        _UNREACHABLE_F( "Not implemented." );
    }

    size_t __mstring_strcspn( const __mstring * /* dest */, const __mstring * /* src */ ) {
        _UNREACHABLE_F( "Not implemented." );
    }

    __mstring * __mstring_strpbrk( const __mstring * /* dest */, const char * /* breakset */ ) {
        _UNREACHABLE_F( "Not implemented." );
    }

    __mstring * __mstring_strstr( const __mstring * /* str */, const char * /*substr */ ) {
        _UNREACHABLE_F( "Not implemented." );
    }

    __mstring * __mstring_strtok( __mstring * /* str */, const char * /* delim */ ) {
        _UNREACHABLE_F( "Not implemented." );
    }

    _MSTRING __mstring * __mstring_undef_value() {
        _UNREACHABLE_F( "Invalid use of mstring value." );
    }

    void __mstring_cleanup(void) {
        abstract::mstring::cleanup( __dios_this_frame()->parent );
    }

    void __mstring_stash( __mstring * str ) {
        if ( str )
            str->refcount_increment();
        __lart_stash( reinterpret_cast< uintptr_t >( str ) );
    }

    __mstring * __mstring_unstash() {
        return reinterpret_cast< __mstring * >( __lart_unstash() );
    }
}

void Quintuple::strcpy(const Quintuple * other) noexcept {
    if (this != other) {
        size_t terminator = other->_terminators.front();

        if ( _buff.size() < terminator + 1 ) {
            assert( false && "copying mstring to smaller mstring" );
        }

        for ( size_t i = _from; i <= terminator; ++i ) {
            safe_set( i, other->_buff[i] );
        }
    }
}

void Quintuple::strcat( const Quintuple * other ) noexcept {
    size_t begin = _terminators.front();
    size_t end = other->_terminators.front() + 1;

    if ( _buff.size() < begin + end ) {
        assert( false && "concating mstring into smaller mstring" );
    }

    for ( size_t i = begin; i < end; ++i ) {
        safe_set( i, other->_buff[i] );
    }
}

Quintuple * Quintuple::strchr( char ch ) const noexcept {
    auto split = this->split();
    auto interest = split.sections().front();

    if ( !interest.empty() ) {
        const auto &begin = interest.segments().begin();
        const auto &end = interest.segments().end();

        auto search = std::find_if( begin, end, [ch] (const auto &seg) {
            return seg.value() == ch;
        });

        if ( search != end ) {
            return __new< Quintuple >( _VM_PT_Marked, const_cast< Quintuple * >( this ), search->from() );
        } else {
            return nullptr;
        }
    } else {
        _UNREACHABLE_F("Error: there's no string of interest!");
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

void Quintuple::set( size_t idx, char val ) noexcept {
    auto split = this->split();
    auto sq = split.sections().front();

    if ( !sq.empty() ) {
        assert( idx >= _buff.from() && idx < _buff.from() + _buff.size() );
        safe_set( idx, val );
    } else {
        _UNREACHABLE_F( "Error: there is no string of interest." );
    }
}

void Quintuple::safe_set( size_t idx, char val ) noexcept {
    if ( _buff[ idx ] == '\0' ) {
        auto it = std::lower_bound( _terminators.begin(), _terminators.end(), idx );
        _terminators.erase( it );
    }
    _buff[ idx ] = val;
    if ( val == '\0' ) {
        auto it = std::lower_bound( _terminators.begin(), _terminators.end(), idx );
        if ( it == _terminators.end() ) {
            _terminators.push_back( idx );
        } else {
            _terminators.insert( it, idx );
        }
    }
}

void mstring_release( Quintuple * quintuple ) noexcept {
    quintuple->refcount_decrement();
}

void mstring_cleanup( Quintuple * quintuple ) noexcept {
    quintuple->~Quintuple();
    __dios_safe_free( quintuple );
}

void mstring_cleanup_check( Quintuple * quintuple ) noexcept {
    cleanup_check( quintuple, mstring_cleanup );
}

} // namespace abstract::mstring
