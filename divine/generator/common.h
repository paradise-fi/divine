// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/stateallocator.h>
#include <divine/blob.h>

#ifndef DIVINE_GENERATOR_COMMON_H
#define DIVINE_GENERATOR_COMMON_H

namespace divine {
namespace generator {

struct Allocator : StateAllocator {
    Pool _pool;
    int _slack;

    Allocator() : _slack( 0 ) {}

    void setSlack( int s ) { _slack = s; }
    Pool &pool() { return _pool; }

    virtual state_t legacy_state( Blob b ) {
        divine::state_t s;
        s.size = b.size() - _slack;
        s.ptr = b.data() + _slack;
        return s;
    }

    virtual Blob unlegacy_state( state_t s ) {
        return Blob( s.ptr - _slack, true );
    }

    state_t duplicate_state( const state_t &st ) {
        Blob a = unlegacy_state( st ), b( pool(), st.size + _slack );
        a.copyTo( b );
        if ( _slack )
            b.clear( 0, _slack );
        return legacy_state( b );
    }

    state_t new_state( const std::size_t size ) {
        Blob b( pool(), size + _slack );
        b.clear();
        return legacy_state( b );
    }

    Blob new_blob( std::size_t size ) {
        Blob b = Blob( pool(), size + _slack );
        b.clear();
        return b;
    }

    void delete_state( state_t &st ) {
        Blob a( st.ptr, true );
        a.free( pool() );
    }
};

struct Common {
    Allocator alloc;
    void setSlack( int s ) { alloc.setSlack( s ); }
    Pool &pool() { return alloc.pool(); }
};

}
}

#endif
