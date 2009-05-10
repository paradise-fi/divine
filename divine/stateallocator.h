// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>

#include <divine/pool.h>
#include <divine/legacy/system/state.hh>

#ifndef DIVINE_STATEALLOCATOR_H
#define DIVINE_STATEALLOCATOR_H

namespace divine {

struct StateAllocator {
    virtual state_t duplicate_state( const state_t &state ) = 0;
    virtual state_t new_state( std::size_t size ) = 0;
    virtual void delete_state( state_t &st ) = 0;
    virtual ~StateAllocator() {}
};

}

#endif
