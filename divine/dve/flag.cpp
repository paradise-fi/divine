// -*- C++ -*- (c) 2011 Jan Kriho

#include <divine/dve/flag.h>
#include <iostream>

namespace divine {
namespace dve {

const ErrorState ErrorState::e_overflow = {1};
const ErrorState ErrorState::e_arrayCheck = {2};
const ErrorState ErrorState::e_divByZero = {4};
const ErrorState ErrorState::e_other = {8};
const ErrorState ErrorState::e_none = {0};

std::ostream &operator<<( std::ostream &o, const ErrorState &err ) {
    o << "ERR: ";
    if ( err.overflow() )
        o << "Overflow, ";
    if ( err.arrayCheck() )
        o << "Out of bounds, ";
    if ( err.divByZero() )
        o << "Division by zero, ";
    if ( err.other() )
        o << "Unknown, ";
    return o;
}

const StateFlags StateFlags::f_commited = {{{1, 0, 0}}};
const StateFlags StateFlags::f_commited_dirty = {{{0, 1, 0}}};
const StateFlags StateFlags::f_none = {{{0, 0, 0}}};

std::ostream &operator<<( std::ostream &o, const StateFlags &flags ) {
    o << "Flags: ";
    if ( flags.f.commited )
        o << "Commited, ";
    if ( flags.f.commited_dirty )
        o << "Dirty Commited, ";
    return o;
}

}    
}
