// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>

#include <divine/toolkit/pool.h>
#include <divine/toolkit/blob.h>

#ifndef DIVINE_ALLOCATOR_H
#define DIVINE_ALLOCATOR_H

namespace divine {

struct Allocator {
    Pool _pool;
    int _slack;

    Allocator() : _slack( 0 ) {}

    void setSlack( int s ) { _slack = s; }
    Pool &pool() { return _pool; }

    Blob new_blob( std::size_t size ) {
        Blob b = Blob( pool(), size + _slack );
        pool().clear( b );
        return b;
    }
};

}

#endif
