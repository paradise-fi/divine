// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>

#include <divine/toolkit/pool.h>
#include <divine/toolkit/blob.h>
#include <divine/legacy/system/state.hh>

#ifndef DIVINE_ALLOCATOR_H
#define DIVINE_ALLOCATOR_H

namespace divine {

struct LegacyAllocator {
#ifdef O_LEGACY
    virtual state_t duplicate_state( const state_t &state ) = 0;
    virtual state_t new_state( std::size_t size ) = 0;
    virtual void delete_state( state_t &st ) = 0;
    virtual ~LegacyAllocator() {}
#endif
};

struct Allocator : LegacyAllocator {
    Pool _pool;
    int _slack;

    Allocator() : _slack( 0 ) {}

    void setSlack( int s ) { _slack = s; }
    Pool &pool() { return _pool; }

    Blob new_blob( std::size_t size ) {
        Blob b = Blob( pool(), size + _slack );
        b.clear();
        return b;
    }

#ifdef O_LEGACY
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

    void delete_state( state_t &st ) {
        Blob a( st.ptr, true );
        a.free( pool() );
    }
#endif
};

}

#endif
