// -*- C++ -*- (c) 2011 Jiri Appl <jiri@appl.name>

#include <bitset>

#ifndef DIVINE_COMPACTSTATE_H
#define DIVINE_COMPACTSTATE_H

namespace divine {

typedef unsigned StateId;
typedef std::bitset< sizeof( unsigned long ) > AC;

/// Represents a state in compact representation
struct CompactState
{
    unsigned forward; // index in forward transitions array of the first forward transition
    unsigned backward; // index in backward transitions array of the first backward transition
    AC ac; // acceptance condition
};

}

#endif
