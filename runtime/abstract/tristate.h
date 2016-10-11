#ifndef __ABSTRACT_TRISTATE_H_
#define __ABSTRACT_TRISTATE_H_

#include "common.h"

#define _ROOT __attribute__((__annotate__("brick.llvm.prune.root")))

namespace abstract {

struct Tristate {
    enum Domain { False = 0, True = 1, Unknown = 2 };
    Domain value = Domain::Unknown;
};

inline Tristate * __abstract_tristate_construct() {
    return allocate< Tristate >();
}

inline Tristate * __abstract_tristate_construct( bool b ) {
    Tristate * obj = allocate< Tristate >();
    obj->value = b ? Tristate::Domain::True : Tristate::Domain::False;
    return obj;
}

inline Tristate * __abstract_tristate_negate( Tristate * t ) {
    if ( t->value == Tristate::Domain::True )
        t->value = Tristate::Domain::False;
    else if ( t->value == Tristate::Domain::False )
        t->value = Tristate::Domain::True;
    //if unknonwn do nothing
    return t;
}

inline Tristate * __abstract_tristate_and( Tristate * a, Tristate * b ) {
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

inline Tristate * __abstract_tristate_or( Tristate * a, Tristate * b ) {
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

    bool __abstract_tristate_explicate( Tristate * tristate ) _ROOT {
        return tristate->value;
    }
}

} // namespace abstract

#endif //__ABSTRACT_TRISTATE_H_
