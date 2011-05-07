// -*- C++ -*- (c) 2011 Jiri Appl <jiri@appl.name>

#ifndef DIVINE_PROBABILISTICTRANSITION_H
#define DIVINE_PROBABILISTICTRANSITION_H

namespace divine {

/// Represents a possible probabilistic transition
struct ProbabilisticTransition {
    unsigned id:8;
    unsigned weight:8;
    unsigned sum:15;
    bool isProbabilistic:1;
    
    ProbabilisticTransition() : id( 0 ), weight( 0 ), sum( 0 ), isProbabilistic( 0 ) {}
};

}

#endif
