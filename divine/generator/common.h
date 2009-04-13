// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/state.h>

#ifndef DIVINE_GENERATOR_COMMON_H
#define DIVINE_GENERATOR_COMMON_H

namespace divine {
namespace generator {

struct Common {
    BlobAllocator alloc;
    void setSlack( int s ) { alloc.setSlack( s ); }
    void setPool( Pool &p ) { alloc.setPool( p ); }
    Pool &pool() { return alloc.pool(); }
};

}
}

#endif
