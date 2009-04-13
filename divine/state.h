// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net

#include <divine/legacy/system/state.hh>
#include <divine/stateallocator.h>
#include <divine/blob.h>

#ifndef DIVINE_STATE_H
#define DIVINE_STATE_H

namespace divine {

/* inline divine::state_t legacy_state( Blob b )
{
    divine::state_t s;
    s.size = b.size();
    s.ptr = b.data();
    return s;
    } */

inline divine::state_t legacy_state( Blob b, int slack )
{
    divine::state_t s;
    s.size = b.size() - slack;
    s.ptr = b.data() + slack;
    return s;
}

struct BlobAllocator : StateAllocator {
    Allocator< char > *_a;
    int _slack;

    BlobAllocator() : _a( 0 ), _slack( 0 ) {}

    void setSlack( int s ) { _slack = s; }

    Allocator< char > *alloc() {
        if ( !_a )
            _a = new Allocator< char >();
        return _a;
    }

    virtual state_t legacy_state( Blob b ) {
        return divine::legacy_state( b, _slack );
    }

    virtual Blob unlegacy_state( state_t s ) {
        return Blob( s.ptr - _slack, true );
    }

    state_t duplicate_state( const state_t &st ) {
        Blob a = unlegacy_state( st ), b( alloc(), st.size + _slack );
        a.copyTo( b );
        return legacy_state( b );
    }

    state_t new_state( const std::size_t size ) {
        Blob b( alloc(), size + _slack );
        b.clear();
        return legacy_state( b );
    }

    Blob new_blob( std::size_t size ) {
        return Blob( alloc(), size + _slack );
    }

    void delete_state( state_t &st ) {
        Blob a( st.ptr, true );
        a.free( alloc() );
    }
};

}

#endif
