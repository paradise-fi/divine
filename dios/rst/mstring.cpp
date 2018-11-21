#include <rst/mstring.h>

#include <rst/common.h>

using namespace abstract::mstring;
using abstract::mark;
using abstract::__new;

Quintuple * __mstring_lift( const char * buff, unsigned buff_len ) {
    return __new< Quintuple >( buff, buff_len );
}

extern "C" {
    _MSTRING const char * __mstring_val( const char * buff, unsigned buff_len ) {
        auto val = __mstring_lift( buff, buff_len ); // TODO copy str
        __lart_stash( reinterpret_cast< uintptr_t >( val ) );
        return abstract::__taint< const char * >( buff );
    }

    size_t __mstring_strlen( const Quintuple * str ) {
        return str->strlen();
    }
}


char * Quintuple::rho() const {
    return nullptr;
}

size_t Quintuple::strlen() const {
    if ( str == nullptr )
        return -1;
    return end;
}
