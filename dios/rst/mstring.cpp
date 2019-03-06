#include <rst/mstring.h>

#include <rst/common.h>
#include <rst/lart.h>

namespace abstract::mstring {

using abstract::__new;

__mstring * __mstring_lift( const char * buff, unsigned buff_len ) {
    return __new< __mstring >( _VM_PT_Marked, buff, buff_len );
}

extern "C" {
    _MSTRING char * __mstring_val( char * buff, unsigned buff_len ) _LART_IGNORE_ARG {
        auto val = __mstring_lift( buff, buff_len );
        poke_object< __mstring >( val, buff );
        __lart_stash( reinterpret_cast< uintptr_t >( val ) );
        return abstract::taint< char * >( buff );
    }

    void __mstring_store( char val, void * addr ) {
         __mstring * str = peek_object< __mstring >( object( addr ) );
         str->write( offset( addr ), val );
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

Section & Split::interest() noexcept
{
    ASSERT( well_formed() );
    return _sections.front();
}

const Section & Split::interest() const noexcept
{
    ASSERT( well_formed() );
    return _sections.front();
}

bool Split::well_formed() const noexcept
{
    return this->empty() || _sections.front().to() < _len;
}

void Split::strcpy( const Split * other ) noexcept
{
    if ( this != other ) {
        auto str = other->interest();
        // TODO check correct bounds
        ASSERT( str.size() < _len && "copying mstring to smaller mstring" );
        // TODO copy segments
        _UNREACHABLE_F( "Not implemented." );
    }
}

void Split::strcat( const Split * other ) noexcept
{
    size_t begin = interest().size();
    size_t dist = other->interest().size() + 1;

    // TODO check correct bounds
    ASSERT( _len >= begin + dist && "concating mstring into smaller mstring" );
    // TODO copy segments
    _UNREACHABLE_F( "Not implemented." );
}

Split * Split::strchr( char ch ) const noexcept
{
    auto str = interest();

    if ( !str.empty() ) {
        const auto &begin = str.segments().begin();
        const auto &end = str.segments().end();

        auto search = std::find_if( begin, end, [ch] ( const auto &seg ) {
            return seg.value() == ch;
        });

        if ( search != end ) {
            // TODO return subsplit
            // return __new< Split >( _VM_PT_Marked, const_cast< Split * >( this ), search->from() );
            _UNREACHABLE_F( "Not implemented." );
        } else {
            return nullptr;
        }
    } else {
        _UNREACHABLE_F("Error: there's no string of interest!");
    }
}

size_t Split::strlen() const noexcept
{
    if ( empty() )
        return 0;
    return interest().size();
}

int Split::strcmp( const Split * other ) const noexcept
{
    const auto & sq1 = interest();
    const auto & sq2 = other->interest();

    if ( !sq1.empty() && !sq2.empty() ) {
        for ( size_t i = 0; i < sq1.size(); i++ ) {
            const auto& sq1_seg = sq1.segment_of( i ); // TODO optimize segment_of
            const auto& sq2_seg = sq2.segment_of( i );

           // TODO optimize per segment comparison
            if ( sq1_seg.value() == sq2_seg.value() ) {
                if ( sq1_seg.to() > sq2_seg.to() ) {
                    return sq1_seg.value() - sq2.segment_of( i + 1 ).value();
                } else if (sq1_seg.to() < sq2_seg.to() ) {
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

Section & Split::section_of( int /*idx*/ ) noexcept
{
    _UNREACHABLE_F( "Not implemented." );
}

const Section & Split::section_of( int /*idx*/ ) const noexcept
{
    _UNREACHABLE_F( "Not implemented." );
}

void Split::write( size_t idx, char val ) noexcept
{
    ASSERT( idx < _len );
    // TODO check value
    // if ( _buff[ idx ] == '\0' ) {
        // TODO merge sections
    // }
    // TODO change value
    //_buff[ idx ] = val;
    if ( val == '\0' ) {
        // TODO split sections
    }

    _UNREACHABLE_F( "Not implemented." );
}

char Split::read( size_t idx ) noexcept
{
    ASSERT( idx < _len );
    _UNREACHABLE_F( "Not implemented." );
}

void split_release( Split * str ) noexcept
{
    str->refcount_decrement();
}

void split_cleanup( Split * str ) noexcept
{
    str->~Split();
    __dios_safe_free( str );
}

void split_cleanup_check( Split * str ) noexcept
{
    cleanup_check( str, split_cleanup );
}

} // namespace abstract::mstring
