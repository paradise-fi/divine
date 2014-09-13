// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net

#include <cstdint>

#include <wibble/test.h> // for assert*
#include <wibble/string.h>

#include <divine/toolkit/pool.h>
#include <divine/toolkit/rpc.h>

#ifndef DIVINE_BLOB_H
#define DIVINE_BLOB_H

namespace wibble {
namespace str {

template<>
inline std::string fmt( const divine::UnBlob &b ) {
    divine::bitstream bs;
    bs.pool = &b.p;
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
