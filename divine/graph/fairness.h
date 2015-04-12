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
    typedef typename St::Vertex Vertex;

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

    Extension &extension( Vertex n ) {
        return n.template extension< Extension >( m_algslack );
    }

    Extension &extension( Node n ) {
        return this->pool().template get< Extension >( n, m_algslack );
    }


    template< typename Yield >
    void withCopy( Node n, Label l, int copy, Yield yield ) {
        this->extension( n ).copy = copy;
        yield( n, l );
    }

    std::string showNode( Node n ) {
        std::stringstream str;
        str << this->base().showNode( n ) << "fairness id = " << int( extension( n ).copy ) << std::endl;
        return str.str();
    }

    template< typename Alloc, typename Yield >
    void successors( Alloc alloc, Vertex stV, Yield yield ) {
        Node st = stV.node();
        int procs = this->base().processCount( st );

        int copy = extension( stV ).copy;
        bool accepting = !!this->base().stateFlags( st, graph::flags::isAccepting );
        ASSERT_LEQ( 0, copy );
        copy = std::min( copy, procs );

        // 0-th copy: transitions from accepting states are redirected to copy 1, other remain unchanged
        if ( copy == 0 ) {
            this->base().successors( alloc, st, [&] ( Node n, Label l ) {
                    this->withCopy( n, l, accepting ? 1 : 0, yield ); } );
        } else { // i-th copy: redirect all transitions made by (i-1)-th process to copy (i+1)
            bool enabled = false;
            do {
                this->base().processSuccessors( alloc, st, [&] ( Node n, Label l ) {
                        this->withCopy( n, l, copy, yield ); }, copy - 1, false );
                this->base().processSuccessors( alloc, st, [&] ( Node n, Label l ) {
                        enabled = true;
                        this->withCopy( n, l, ( copy + 1 ) % ( procs + 1 ), yield ); },
                    copy - 1, true );
                copy = ( copy + 1 ) % ( procs + 1 );
                // if process is disabled, epsilon-step to the next copy can be made
            } while ( !enabled && copy > 0 );

            if ( !enabled ) { // break the epsilon-chain to avoid skipping the copy 0
                this->base().successors( alloc, st, [&] ( Node n, Label l ) {
                        this->withCopy( n, l, copy, yield ); } );
            }
        }
    }

    // only states in copy 0 can be accepting
    template< typename QueryFlags >
    graph::FlagVector stateFlags( Node s, QueryFlags qf ) {
        auto base = this->base().stateFlags( s, qf );
        graph::FlagVector out;
        for ( auto f : base )
            if ( f != graph::flags::accepting || extension( s ).copy == 0 )
                out.emplace_back( f );
        return out;
    }
};

}
}

#endif
