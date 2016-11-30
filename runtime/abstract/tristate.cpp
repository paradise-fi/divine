#include "tristate.h"

#ifdef __divine__
#include <divine.h>
#else
#include <cstdlib>
#endif

#include "common.h"

#define _ROOT __attribute__((__annotate__("brick.llvm.prune.root")))

namespace abstract {

Tristate * __abstract_tristate_construct( Tristate::Domain value ) {
    auto obj = allocate< Tristate >();
    obj->value = value;
    return obj;
}

Tristate * __abstract_tristate_construct() {
    return __abstract_tristate_construct( Tristate::Domain::Unknown );
}

Tristate * __abstract_tristate_construct( bool b ) {
    auto value =  b ? Tristate::Domain::True : Tristate::Domain::False;
    return __abstract_tristate_construct( value );
}

Tristate * __abstract_tristate_negate( Tristate * t ) {
    if ( t->value == Tristate::Domain::True )
        t->value = Tristate::Domain::False;
    else if ( t->value == Tristate::Domain::False )
        t->value = Tristate::Domain::True;
    //if unknonwn do nothing
    return t;
}

Tristate * __abstract_tristate_and( Tristate * a, Tristate * b ) {
    if ( a->value == Tristate::Domain::Unknown )
        return a;
    if ( b->value == Tristate::Domain::Unknown )
        return b;
    if ( a->value == Tristate::Domain::False )
        return a;
    if ( b->value == Tristate::Domain::False )
        return b;
    return a;
}

Tristate * __abstract_tristate_or( Tristate * a, Tristate * b ) {
    if ( a->value == Tristate::Domain::True )
        return a;
    if ( b->value == Tristate::Domain::True )
        return b;
    if ( a->value == Tristate::Domain::Unknown )
        return a;
    if ( b->value == Tristate::Domain::Unknown )
        return b;
    return a;
}

extern "C" {
    Tristate * __abstract_tristate_create() _ROOT {
        return __abstract_tristate_construct();
    }

    Tristate * __abstract_tristate_lift( bool b ) _ROOT {
        return __abstract_tristate_construct( b );
    }

    bool __abstract_tristate_lower( Tristate * tristate ) _ROOT {
        if ( tristate->value == Tristate::Domain::Unknown ) {
#ifdef __divine__
            return __vm_choose( 2 );
#else
            return std::rand() % 2;
#endif
        }
        return tristate->value;
    }
}

} // namespace abstract
