// -*- C++ -*- (c) 2009, 2012 Petr Rockai <me@mornfall.net>

#include <divine/graph/graph.h>

#ifndef DIVINE_GRAPH_POR_H
#define DIVINE_GRAPH_POR_H

namespace divine {
namespace graph {

template< typename G, typename St >
struct NonPORGraph : graph::Transform< G > {

    typedef typename G::Node Node;
    typedef typename St::Vertex Vertex;
    typedef typename St::VertexId VertexId;
    typedef typename St::QueueVertex QueueVertex;
    typedef void (*UpdatePredCount)( Pool&, VertexId, int );

    void porExpansion( Vertex ) {}
    void porTransition( Vertex, Vertex, int* ) {}
    bool full( Node ) { return true; }

    template< typename Yield >
    void fullexpand( Yield, Node ) {}

    template< typename Algorithm >
    void _porEliminate( Algorithm &, UpdatePredCount ) {}

    template< typename Domain, typename Alg >
    bool porEliminate( Domain &, Alg & ) {
        return false;
    }

    template< typename Table >
    bool porEliminateLocally( Table &, UpdatePredCount ) {
        return false;
    }

    template< typename Yield >
    void porExpand( St&, Yield yield ) {}


    template< typename Yield >
    void successors( Vertex st, Yield yield ) {
        this->base().successors( st.getNode( this->base().alloc.pool() ), yield );
    }
};

}
}

#endif
