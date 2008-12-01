// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net

#include <divine/legacy/system/state.hh>
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

}

#endif
