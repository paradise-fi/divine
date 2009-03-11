// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/config.h>
#include <divine/blob.h>

#ifndef DIVINE_ALGORITHM_H
#define DIVINE_ALGORITHM_H

namespace divine {
namespace algorithm {

template< typename T >
struct _MpiId
{
    static int to_id( void (T::*f)() ) {
        // assert( 0 );
        return -1;
    }

    static void (T::*from_id( int ))() {
        // assert( 0 );
        return 0;
    }

    template< typename O >
    static void writeShared( typename T::Shared, O ) {
    }

    template< typename I >
    static I readShared( typename T::Shared &, I i ) {
        return i;
    }
};

inline int workerCount( Config *c ) {
    if ( !c )
        return 1;
    return c->workers();
}

struct Hasher {
    int size;

    Hasher( int s = 0 ) : size( s ) {}
    void setSize( int s ) { size = s; }

    inline hash_t operator()( Blob b ) const {
        return size ? b.hash( 0, size ) : b.hash();
    }
};

struct Equal {
    int size;
    Equal( int s = 0 ) : size( s ) {}
    inline hash_t operator()( Blob a, Blob b ) const {
        return a.compare( b, 0, size ) == 0;
    }
};

struct Algorithm
{
    void resultBanner( bool valid ) {
        std::cerr << " ===================================== " << std::endl
                  << ( valid ?
                     "       Accepting cycle NOT found       " :
                     "         Accepting cycle FOUND         " )
                  << std::endl
                  << " ===================================== " << std::endl;
    }
};

}
}

#endif
