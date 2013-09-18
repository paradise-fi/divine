// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net

#include <cstdint>

#include <wibble/test.h> // for assert*
#include <divine/toolkit/pool.h>

#ifndef DIVINE_BLOB_H
#define DIVINE_BLOB_H

struct TestBlob;

namespace divine {

/* template< template< typename > class M, typename T, typename F >
   using FMap = M< typename std::result_of< F( T ) > >;

template< typename T >
struct Unwrap {};

template< typename T >
struct Wrap {
    T _wrapped;
};

template< typename T >
struct Unwrap< Wrap< T > > {
};

template< typename T >
Unwrap< T >::Blob unwrap( const T &t ) {
    Unwrap< T >::blob( t );
}
*/

}

#include <wibble/string.h>
#include <divine/toolkit/bitstream.h>

namespace wibble {
namespace str {

template<>
inline std::string fmt( const divine::UnBlob &b ) {
    divine::bitstream bs( b.p );
    bs << b.b;
    bs.bits.pop_front(); /* remove size */
    return fmt( bs.bits );
}

}
}

namespace divine {
inline std::string fmtblob( Pool &p, Blob b ) {
    return wibble::str::fmt( UnBlob( p, b ) );
}
}

#endif
