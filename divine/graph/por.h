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

    void porExpansion( Vertex ) {}
    void porTransition( Vertex, Vertex, void (*)( Pool&, Vertex, int ) ) {}
    bool full( Node ) { return true; }

    template< typename Yield >
    void fullexpand( Yield, Node ) {}

    template< typename Algorithm >
    void _porEliminate( Algorithm & ) {}

    template< typename Domain, typename Alg >
    bool porEliminate( Domain &, Alg & ) {
        return false;
    }

    template< typename Table >
    bool porEliminateLocally( Table & ) {
        return false;
    }

    template< typename Yield >
    void porExpand( Yield yield ) {}
};

}
}

#endif
