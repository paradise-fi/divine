#include <rst/mstring.h>

#include <rst/domains.h>
#include <rst/common.h>
#include <rst/lart.h>

namespace abstract::mstring {

using abstract::__new;

__mstring * __mstring_lift( const char * buff, unsigned buff_len ) {
    return __new< __mstring >( _VM_PT_Heap, buff, buff_len );
}

extern "C" {
    _MSTRING char * __mstring_val( char * buff, unsigned buff_len ) _LART_IGNORE_ARG {
        auto val = __mstring_lift( buff, buff_len );
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

} // namespace abstract::mstring
