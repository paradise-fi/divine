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
    typedef typename St::Handle Handle;

    void porExpansion( Vertex ) {}
    void porTransition( St &, Vertex, Vertex ) {}
    bool full( Vertex ) { return true; }

    template< typename Alloc, typename Yield >
    void fullexpand( Alloc, Yield, Vertex ) { }

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

    template< typename Alloc, typename Yield >
    void porExpand( Alloc, St&, Yield ) {}
};

}
}

#endif
