// -*- C++ -*-

#include <vector>
#include <divine/graph/por.h>

#ifndef DIVINE_GRAPH_FAIRNESS_H
#define DIVINE_GRAPH_FAIRNESS_H

namespace divine {
namespace graph {

template< typename G, typename St >
struct FairGraph : NonPORGraph< G, St > {

    typedef typename G::Node Node;
    typedef typename G::Label Label;

    int m_algslack;

    struct Extension {
        unsigned char copy;
    };

    int setSlack( int s ) {
        m_algslack = s;
        // copy id has to be treated as a part of the state information
        NonPORGraph< G, St >::setSlack( s + sizeof( Extension ) );
        return s;
    }

    Extension &extension( Node n ) {
        return this->base().alloc.pool().template get< Extension >( n, m_algslack );
    }

    template< typename Yield >
    void withCopy( Node n, Label l, int copy, Yield yield ) {
        this->extension( n ).copy = copy;
        yield( n, l );
    }

    template< typename Yield >
    void successors( Node st, Yield yield ) {
        int procs = this->base().processCount();

        int copy = extension( st ).copy;
        bool accepting = this->base().isAccepting( st );
        assert_leq( 0, copy );
        assert_leq( copy, procs );

        // 0-th copy: transitions from accepting states are redirected to copy 1, other remain unchanged
        if ( copy == 0 ) {
            this->base().successors( st, [&] ( Node n, Label l ) {
                    this->withCopy( n, l, accepting ? 1 : 0, yield ); } );
        } else { // i-th copy: redirect all transitions made by (i-1)-th process to copy (i+1)
            bool enabled = false;
            do {
                this->base().processSuccessors( st, [&] ( Node n, Label l ) {
                        this->withCopy( n, l, copy, yield ); }, copy - 1, false );
                this->base().processSuccessors( st, [&] ( Node n, Label l ) {
                        enabled = true;
                        this->withCopy( n, l, ( copy + 1 ) % ( procs + 1 ), yield ); },
                    copy - 1, true );
                copy = ( copy + 1 ) % ( procs + 1 );
                // if process is disabled, epsilon-step to the next copy can be made
            } while ( !enabled && copy > 0 );

            if ( !enabled ) { // break the epsilon-chain to avoid skipping the copy 0
                this->base().successors( st, [&] ( Node n, Label l ) {
                        this->withCopy( n, l, copy, yield ); } );
            }
        }
    }

    // only states in copy 0 can be accepting
    bool isAccepting( Node s ) {
        return extension( s ).copy == 0 && this->base().isAccepting( s );
    }

    bool isInAccepting( Node s, int acc_group ) {
        return extension( s ).copy == 0 && this->base().isInAccepting( s, acc_group );
    }

    template< typename Alg >
    int successorNum( Alg &a, Node current, Node next, unsigned fromIndex = 0 ) {
        // successorNum works on the original graph, so all successors are in copy 0
        // therefore, if 'next' is not in copy 0, it is not found among the successors
        int orig = extension( next ).copy;
        extension( next ).copy = 0;
        int ret = NonPORGraph< G, St >::successorNum( a, current, next, fromIndex );
        extension( next ).copy = orig;
        return ret;
    }
};

}
}

#endif
