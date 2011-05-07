// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/allocator.h>
#include <divine/blob.h>
#include <divine/hashset.h>
#include <divine/graph.h>
#include <divine/algorithm/common.h>

#ifndef DIVINE_GENERATOR_COMMON_H
#define DIVINE_GENERATOR_COMMON_H

namespace divine {
namespace generator {

using namespace graph;

template< typename _Node >
struct Common : graph::Base< _Node > {
    typedef _Node Node;
    /// Default storage for the visited set. May be overriden by subclasses.
    typedef HashSet< Node, algorithm::Hasher, divine::valid< Node >, algorithm::Equal > Table;
};

template< typename G >
struct Extended {
    G _g;

    typedef typename G::Node Node;
    typedef typename G::Successors Successors;
    typedef G Graph;

    G &g() { return _g; }

    Node initial() { return g().initial(); }
    Successors successors( Node st ) { return g().successors( st ); }
    void release( Node s ) { g().release( s ); }
    bool isDeadlock( Node s ) { return g().isDeadlock( s ); }
    bool isGoal( Node s ) { return g().isGoal( s ); }
    bool isAccepting( Node s ) { return g().isAccepting( s ); }
    std::string showNode( Node s ) { return g().showNode( s ); }
    void read( std::string path ) { g().read( path ); }
    void setDomainSize( const unsigned mpiRank = 0, const unsigned mpiSize = 1,
                        const unsigned peersCount = 1 ) {
        g().setDomainSize( mpiRank, mpiSize, peersCount );
    }

    PropertyType propertyType() { return g().propertyType(); }

    bool isInAccepting( Node s, const size_int_t acc_group ) { return g().isInAccepting( s, acc_group ); }
    bool isInRejecting( Node s, const size_int_t acc_group ) { return g().isInRejecting( s, acc_group ); }
    unsigned acceptingGroupCount() { return g().acceptingGroupCount(); }

    template< typename Q >
    void queueInitials( Q &q ) {
        g().queueInitials( q );
    }

    int setSlack( int s ) { return g().setSlack( s ); }

    /// Returns an owner id of the state n
    template< typename Hash, typename Worker >
    int owner( Hash &hash, Worker &worker, Node n, hash_t hint = 0 ) {
        return g().owner( hash, worker, n, hint );
    }

    /// Returns a position between 1 and n
    template< typename Alg >
    int successorNum( Alg &a, Node current, Node next )
    {
        typename G::Successors succ = g().successors( current );
        int edge = 0;
        while ( !succ.empty() ) {
            ++ edge;
            if ( a.equal( succ.head(), next ) )
                break;
            g().release( succ.head() ); // not it
            succ = succ.tail();
            assert( !succ.empty() ); // we'd hope the node is actually there!
        }

        while ( !succ.empty() ) {
            g().release( succ.head() );
            succ = succ.tail();
        }

        assert_leq( 1, edge );
        return edge;
    }
};

}
}

#endif
