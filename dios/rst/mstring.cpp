#include <rst/mstring.h>

#include <rst/common.h>

using namespace abstract::mstring;
using abstract::mark;
using abstract::__new;

Quintuple * __mstring_lift( const char * str ) {
    return mark( __new< Quintuple >( str ) );
}

extern "C" {
    _MSTRING char * __mstring_val( const char * str ) {
        auto val = __mstring_lift( str ); // TODO copy str
        __lart_stash( reinterpret_cast< uintptr_t >( val ) );
        return abstract::__taint< char * >();
    }
}
