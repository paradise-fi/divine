#include <rst/mstring.h>

#include <rst/domains.h>
#include <rst/common.h>
#include <rst/lart.h>

namespace abstract::mstring {

using abstract::__new;

extern "C" {
    __mstring * __mstring_lift( const void * buff, unsigned len ) {
        auto str = static_cast< const char * >( buff );
        auto split = __new< __mstring >( _VM_PT_Heap );

        split->append( Bound::singleton( 0 ) );
        if ( len == 0 )
            return split;

        char prev = str[ 0 ];
        for ( unsigned i = 0; i < len; ++i ) {
            if ( prev != str[ i ] ) {
                split->append( prev );
                split->append( Bound::singleton( i ) );
                prev = str[ i ];
            }
        }

        split->append( prev );
        split->append( Bound::singleton( len ) );

        return split;
    }

    _MSTRING char * __mstring_val( char * buff, unsigned len ) _LART_IGNORE_ARG {
        auto val = __mstring_lift( buff, len );
        __lart_stash( reinterpret_cast< uintptr_t >( val ) );
        return abstract::taint< char * >( buff );
    }

    void __mstring_store( char val, __mstring * str ) { str->write( val ); }

    char __mstring_load( __mstring * str ) { return str->read(); }

    __mstring * __mstring_gep( __mstring * addr, uint64_t idx )
    {
        return __new< __mstring >( _VM_PT_Heap,
            addr->_max_size, addr->_offset + sym::constant( idx ), addr->_values, addr->_bounds );
    }

    /* String manipulation */
    __mstring * __mstring_strcpy( __mstring * dst, const __mstring * src )
    {
        return mstring::strcpy( dst, src );
    }

    __mstring * __mstring_strncpy( __mstring * /* dst */, const __mstring * /* src */, size_t  /* count */ )
    {
        _UNREACHABLE_F( "Not implemented." );
    }

    __mstring * __mstring_strcat( __mstring * dst, const __mstring * src )
    {
        return mstring::strcat( dst, src );
    }

    __mstring * __mstring_strncat( __mstring * /* dst */, const __mstring * /* src */, size_t  /* count */ )
    {
        _UNREACHABLE_F( "Not implemented." );
    }

    size_t __mstring_strxfrm( __mstring * /* dst */, const __mstring * /* src */, size_t  /* count */ )
    {
        _UNREACHABLE_F( "Not implemented." );
    }

    /* String examination */
    size_t __mstring_strlen( const __mstring * str ) { return mstring::strlen( str ); }

    int __mstring_strcmp( const __mstring * lhs, const __mstring * rhs )
    {
        return mstring::strcmp( lhs, rhs );
    }

    int __mstring_strncmp( const __mstring * /* lhs */, const __mstring * /* rhs */, size_t  /* count */ )
    {
        _UNREACHABLE_F( "Not implemented." );
    }

    int __mstring_strcoll( const __mstring * /* lhs */, const __mstring * /* rhs */ )
    {
        _UNREACHABLE_F( "Not implemented." );
    }

    __mstring * __mstring_strchr( const __mstring * str, int ch )
    {
        return mstring::strchr( str, ch );
    }

    __mstring * __mstring_strrchr( const __mstring * /* str */, int /* ch */ )
    {
        _UNREACHABLE_F( "Not implemented." );
    }

    size_t __mstring_strspn( const __mstring * /* dst */, const __mstring * /* src */ )
    {
        _UNREACHABLE_F( "Not implemented." );
    }

    size_t __mstring_strcspn( const __mstring * /* dst */, const __mstring * /* src */ )
    {
        _UNREACHABLE_F( "Not implemented." );
    }

    __mstring * __mstring_strpbrk( const __mstring * /* dst */, const char * /* breakset */ )
    {
        _UNREACHABLE_F( "Not implemented." );
    }

    __mstring * __mstring_strstr( const __mstring * /* str */, const char * /*substr */ )
    {
        _UNREACHABLE_F( "Not implemented." );
    }

    __mstring * __mstring_strtok( __mstring * /* str */, const char * /* delim */ )
    {
        _UNREACHABLE_F( "Not implemented." );
    }


    // TODO extract impl
    void __mstring_freeze( __mstring * str, void * addr )
    {
        /*if ( abstract::tainted( *static_cast< char * >( addr ) ) ) {
            auto old = peek_object< __mstring >( addr );
            old->refcount_decrement();
            if ( !old->refcount() ) {
                __vm_poke( addr, _VM_ML_User, 0 );
                mstring_cleanup( old );
            }
        }*/

        if ( str ) {
        //  str->refcount_increment();
            poke_object< __mstring >( str, addr );
        }
    }

    __mstring * __mstring_thaw( void * addr )
    {
        return peek_object< __mstring >( addr );
    }

    __mstring * __mstring_realloc( __mstring * str, size_t size )
    {
        return mstring::realloc( str, size );
    }

    __mstring * __mstring_memcpy( __mstring * dst, __mstring * src, size_t len )
    {
        return mstring::memcpy( dst, src, len );
    }

    __mstring * __mstring_memmove( __mstring * /*dst*/, __mstring * /*src*/, size_t /*len*/ )
    {
        _UNREACHABLE_F( "Not implemented." );
    }

    __mstring * __mstring_memset( __mstring * /*str*/, char /*value*/, size_t /*len*/ )
    {
        _UNREACHABLE_F( "Not implemented." );
    }

    __mstring * __mstring_memchr( __mstring * /*str*/, int /*c*/, size_t /*n*/ )
    {
        _UNREACHABLE_F( "Not implemented." );
    }

    size_t __mstring_fread( __mstring * ptr, size_t size, size_t nmemb, FILE * stream )
    {
        char buff[ size * nmemb ];
        auto ret = fread( buff, size, nmemb, stream );
        for ( unsigned i = 0; i < size * nmemb; ++i ) {
            ptr->write( i, buff[ i ] );
        }
        return ret;
    }

    size_t __mstring_fwrite( __mstring * ptr, size_t size, size_t nmemb, FILE * stream )
    {
        char buff[ size * nmemb ];
        for ( unsigned i = 0; i < size * nmemb; ++i ) {
            buff[ i ] = ptr->read( i );
        }
        return fwrite( buff, size, nmemb, stream );
    }

    void __mstring_stash( __mstring * str )
    {
        __lart_stash( reinterpret_cast< uintptr_t >( str ) );
    }

    __mstring * __mstring_unstash()
    {
        return reinterpret_cast< __mstring * >( __lart_unstash() );
    }
}

} // namespace abstract::mstring
