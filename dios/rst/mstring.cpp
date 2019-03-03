#include <rst/mstring.h>

#include <rst/common.h>
#include <rst/lart.h>

#include <algorithm> // min

namespace abstract::mstring {

using abstract::__new;

__mstring * __mstring_lift( const char * buff, unsigned buff_len ) {
    return __new< Mstring >( _VM_PT_Marked, buff, buff_len, 0, 1 );
}

extern "C" {
    _MSTRING char * __mstring_val( char * buff, unsigned buff_len ) _LART_IGNORE_ARG {
        auto val = __mstring_lift( buff, buff_len );
        poke_object< __mstring >( val, buff );
        __lart_stash( reinterpret_cast< uintptr_t >( val ) );
        return abstract::taint< char * >( buff );
    }

    void __mstring_store( char val, void * addr ) {
         __mstring *mstr = peek_object< __mstring >( object( addr ) );
         mstr->write( offset( addr ), val );
    }

    char __mstring_load( void * addr ) {
        __mstring *mstr = peek_object< __mstring >( object( addr ) );
        return mstr->read( offset( addr ) );
    }

    __mstring * __mstring_gep( __mstring * addr, uint64_t /*idx*/ ) {
        return addr;
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

    // TODO extract impl
    void __mstring_freeze( __mstring * str, void * addr ) {
        /*if ( abstract::tainted( *static_cast< char * >( addr ) ) ) {
            auto old = peek_object< __mstring >( addr );
            old->refcount_decrement();
            if ( !old->refcount() ) {
                __vm_poke( addr, _VM_ML_User, 0 );
                mstring_cleanup( old );
            }
        }*/

        if ( str ) {
            str->refcount_increment();
            poke_object< __mstring >( str, addr );
        }
    }

    __mstring * __mstring_thaw( void * addr ) {
        return peek_object< __mstring >( addr );
    }
}

void Mstring::strcpy(const Mstring * other) noexcept {
    if (this != other) {
        size_t terminator = other->_terminators.front();

        if ( _buff.size() < terminator + 1 ) {
            assert( false && "copying mstring to smaller mstring" );
        }

        for ( size_t i = _from; i <= terminator; ++i ) {
            safe_write( i, other->_buff[i] );
        }
    }
}

void Mstring::strcat( const Mstring * other ) noexcept {
    size_t begin = _terminators.front();
    size_t dist = other->_terminators.front() + 1;

    if ( _buff.size() < begin + dist ) {
        assert( false && "concating mstring into smaller mstring" );
    }

    for ( size_t i = 0; i < dist; ++i ) {
        safe_write( begin + i, other->_buff[i] );
    }
}

Mstring * Mstring::strchr( char ch ) const noexcept {
    auto split = this->split();
    auto interest = split.sections().front();

    if ( !interest.empty() ) {
        const auto &begin = interest.segments().begin();
        const auto &end = interest.segments().end();

        auto search = std::find_if( begin, end, [ch] (const auto &seg) {
            return seg.value() == ch;
        });

        if ( search != end ) {
            return __new< Mstring >( _VM_PT_Marked, const_cast< Mstring * >( this ), search->from() );
        } else {
            return nullptr;
        }
    } else {
        _UNREACHABLE_F("Error: there's no string of interest!");
    }
}

size_t Mstring::strlen() const noexcept {
    if ( _buff.data() == nullptr )
        return -1;
    assert(!_terminators.empty());
    return _terminators.front();
}

auto Mstring::split() const noexcept -> Split< Mstring::Buffer > {
    if ( _buff.data() != nullptr ) {
        assert(!_terminators.empty());
        return Split( _buff, _terminators );
    }
    _UNREACHABLE_F( "Error: Trying to split buffer without a string of interest." );
}

int Mstring::strcmp( const Mstring * other ) const noexcept {
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

void Mstring::write( size_t idx, char val ) noexcept {
    auto split = this->split();
    auto sq = split.sections().front();

    if ( !sq.empty() ) {
        assert( idx >= _buff.from() && idx < _buff.from() + _buff.size() );
        safe_write( idx, val );
    } else {
        _UNREACHABLE_F( "Error: there is no string of interest." );
    }
}

void Mstring::safe_write( size_t idx, char val ) noexcept {
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

char Mstring::read( size_t idx ) noexcept {
    assert( idx >= _buff.from() && idx < _buff.from() + _buff.size() );
    return _buff[ idx ];
}

void mstring_release( Mstring * str ) noexcept {
    str->refcount_decrement();
}

void mstring_cleanup( Mstring * str ) noexcept {
    str->~Mstring();
    __dios_safe_free( str );
}

void mstring_cleanup_check( Mstring * str ) noexcept {
    cleanup_check( str, mstring_cleanup );
}

} // namespace abstract::mstring
