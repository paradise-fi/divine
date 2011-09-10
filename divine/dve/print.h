// -*- C++ -*- (c) 2011 Petr Rockai
#include <divine/dve/parse.h>

#ifndef DIVINE_DVE_PRINT_H
#define DIVINE_DVE_PRINT_H

namespace divine {

using namespace dve::parse;
using namespace dve;

static inline std::ostream &operator<<( std::ostream &o, Identifier i ) {
    return o << i.name();
}

static inline std::ostream &operator<<( std::ostream &o, Constant i ) {
    return o << i.value;
}

inline std::ostream &operator<<( std::ostream &o, const Declaration &d ) {
    if ( d.is_const )
        o << "const ";
    if ( d.width == 8 )
        o << "byte ";
    if ( d.width == 16 )
        o << "int ";

    o << "(...)";
    return o;
}

std::ostream &operator<<(std::ostream &o, const RValue &e) {
    if ( e.ident.valid() )
        return o << e.ident;
    if ( e.value.valid() )
        return o << e.value;
    return o;
}

std::ostream &operator<<(std::ostream &o, const parse::Expression &e) {
    if (e.rval)
        return o << *e.rval;

    if ( e.op.id == Token::Bool_Not ) {
        o << "not ";
        o << "(" << *e.lhs << ")";
    } else if ( e.rhs ) {
        o << "(" << *e.lhs
          << " " << e.op.data
          << " " << *e.rhs << ")";
    }

    return o;
}

}

#endif
