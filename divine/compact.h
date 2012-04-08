// -*- C++ -*- (c) 2011 Jiri Appl <jiri@appl.name>

#include <bitset>

#ifndef DIVINE_COMPACT_H
#define DIVINE_COMPACT_H

namespace divine {

/// Compact file string literals
namespace compact {
    const std::string cfStates = "states:";
    const std::string cfBackward = "backward:";
    const std::string cfForward = "forward:";
    const std::string cfInitial = "initial:";
    const std::string cfAC = "ac:";
    const std::string cfState = "state:";
    const std::string cfProbabilistic = "probabilistic:";
}

typedef unsigned StateId;
typedef std::bitset< sizeof( unsigned long ) > AC;

/// Represents a state in compact representation
struct CompactState {
    unsigned forward; // index in forward transitions array of the first forward transition
    unsigned backward; // index in backward transitions array of the first backward transition
    AC ac; // acceptance condition
};

}

#endif
