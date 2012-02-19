// -*- C++ -*-

#ifndef DIVINE_FAIRGRAPH_H
#define DIVINE_FAIRGRAPH_H

#include <divine/porcp.h>

namespace divine {
namespace algorithm {

template< typename G >
struct FairGraph : NonPORGraph< G > {

    typedef typename G::Node Node;
    typedef typename G::Successors GenSuccs;

    int m_algslack;

    struct Extension {
        unsigned char copy;
    };

    int setSlack( int s ) {
        m_algslack = s;
        // copy id has to be treated as a part of the state information
        NonPORGraph< G >::setSlack( s + sizeof( Extension ) );
        return s;
    }

    Extension &extension( Node n ) {
        return n.template get< Extension >( m_algslack );
    }

    // Successor container consisting of multiple successor containers returned by generator
    struct Successors {
        vector< pair< GenSuccs, int > > parts;
        int current;
        FairGraph *parent;

        Node from() {
            assert( !parts.empty() );
            return parts[ 0 ].first.from();
        }

        bool empty() const {
            return size_t( current ) >= parts.size();
        }

        Node head() const {
            Node n = parts[ current ].first.head();
            parent->extension( n ).copy = parts[ current ].second;
            return n;
        }

        Successors tail() const {
            Successors s = *this;
            if ( empty() ) return s;
            s.parts[ s.current ].first = s.parts[ s.current ].first.tail();
            while ( !s.empty() && s.parts[ s.current ].first.empty() ) s.current++;
            return s;
        }

        // Adds succesor container and directs all contained states to given copy of the statespace
        void add( GenSuccs s, int copy ) {
            if ( size_t( current ) == parts.size() && s.empty() )
                current++;
            parts.push_back( make_pair( s, copy ) );
        }

        Successors() : current( 0 ) {}
    };

    Successors successors( Node st ) {
        int procs = this->base().processCount();
        Successors succs;
        succs.parent = this;

        int copy = extension( st ).copy;
        assert( copy >= 0 && copy <= procs );
        // 0-th copy: transitions from accepting states are redirected to copy 1, other remain unchanged
        if ( copy == 0 ) {
            succs.add( this->base().successors( st ), this->base().isAccepting( st ) ? 1 : 0 );
        }
        // i-th copy: redirect all transitions made by (i-1)-th process to copy (i+1)
        else {
            GenSuccs proc_succs;
            do {
                succs.add( this->base().processSuccessors( st , copy - 1, false ), copy );  // stay in curent copy
                proc_succs = this->base().processSuccessors( st , copy - 1, true );
                copy = ( copy + 1 ) % ( procs + 1 );
                // if process is disabled, epsilon-step to the next copy can be made
            } while ( proc_succs.empty() && copy > 0 );
            if ( proc_succs.empty() ) // break the epsilon-chain to avoid skipping the copy 0
                proc_succs = this->base().successors( st );
            succs.add( proc_succs, copy ); // go to next copy
        }
        return succs;
    }

    // only states in copy 0 can be accepting
    bool isAccepting( Node s ) {
        return extension( s ).copy == 0 && this->base().isAccepting( s );
    }

    bool isInAccepting( Node s, const size_int_t acc_group ) {
        return extension( s ).copy == 0 && this->base().isInAccepting( s, acc_group );
    }

    template< typename Alg >
    int successorNum( Alg &a, Node current, Node next, unsigned fromIndex = 0 ) {
        // successorNum works on the original graph, so all successors are in copy 0
        // therefore, if 'next' is not in copy 0, it is not found among the successors
        int orig = extension( next ).copy;
        extension( next ).copy = 0;
        int ret = NonPORGraph< G >::successorNum( a, current, next, fromIndex );
        extension( next ).copy = orig;
        return ret;
    }
};

}
}

#endif
