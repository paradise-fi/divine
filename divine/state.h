// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net

#include <divine/legacy/system/state.hh>
#include <divine/stateallocator.h>
#include <divine/blob.h>

#ifndef DIVINE_STATE_H
#define DIVINE_STATE_H

namespace divine {

inline divine::state_t legacy_state( Blob b )
{
    divine::state_t s;
    s.size = b.size();
    s.ptr = b.data();
    return s;
}

inline divine::state_t legacy_state( Blob b, int ext )
{
    divine::state_t s;
    s.size = b.size() - ext;
    s.ptr = b.data();
    return s;
}

struct BlobAllocator : StateAllocator {
    Allocator< char > *_a;
    int _ext;

    BlobAllocator( int ext = 0 ) : _a( 0 ), _ext( ext ) {
    }

    Allocator< char > *alloc() {
        if ( !_a )
            _a = new Allocator< char >();
        return _a;
    }

    // GOSH!
    virtual void fixup_state( state_t &st ) {
        st.size -= _ext;
    }

    state_t duplicate_state( const state_t &st ) {
        Blob a( st.ptr, true ), b( alloc(), st.size + _ext );
        a.copyTo( b );
        return legacy_state( b, _ext );
    }

    state_t new_state( const std::size_t size ) {
        Blob b( alloc(), size + _ext );
        b.clear();
        return legacy_state( b, _ext );
    }

    Blob new_blob( std::size_t size ) {
        return Blob( alloc(), size + _ext );
    }

    void delete_state( state_t &st ) {
        Blob a( st.ptr, true );
        a.free( alloc() );
    }
};

}

#endif
