// -*- C++ -*- (c) 2011 Petr Rockai <me@mornfall.net>

#include <divine/allocator.h>

#ifndef DIVINE_GRAPH_H
#define DIVINE_GRAPH_H

namespace divine {
namespace graph {

/// Types of acceptance condition
enum PropertyType { AC_None, AC_Buchi, AC_GenBuchi, AC_Muller, AC_Rabin, AC_Streett };

template< typename _Node >
struct Base {
    typedef _Node Node;

    Allocator alloc;
    int setSlack( int s ) { alloc.setSlack( s ); return s; }
    Pool &pool() { return alloc.pool(); }
    void initPOR() {}

    /// Acceptance condition type. This should be overriden in subclasses.
    PropertyType propertyType() { return AC_None; }

    // for single-set acceptance conditions (Buchi)
    bool isAccepting( Node s ) { return false; }

    // for multi-set acceptance conditions (Rabin, Streett, ...)
    bool isInAccepting( Node s, const size_int_t acc_group ) { return false; }
    bool isInRejecting( Node s, const size_int_t acc_group ) { return false; }
    unsigned acceptingGroupCount() { return 0; }

    // HACK: Inform the graph of the compute domain geometry, required by the
    // Compact generator for more efficient operation
    void setDomainSize( unsigned /* mpiRank */ = 0, unsigned /* mpiSize */ = 1,
                        unsigned /* peersCount */ = 1 ) {}

    template< typename O >
    O getProperties( O o ) {
        return o;
    }

    /// Returns an owner id of the state n
    template< typename Hash, typename Worker >
    int owner( Hash &hash, Worker &worker, Node n, hash_t hint = 0 ) {
        assert( n.valid() );

        if ( !hint )
            return hash( n ) % worker.peers();
        else
            return hint % worker.peers();
    }
};

template< typename G >
struct Transform {
    G _base;

    typedef typename G::Node Node;
    typedef typename G::Successors Successors;
    typedef G Graph;

    G &base() { return _base; }
    Pool &pool() { return base().pool(); }

    Node initial() { return base().initial(); } // XXX remove
    Successors successors( Node st ) { return base().successors( st ); }
    void release( Node s ) { base().release( s ); }
    bool isDeadlock( Node s ) { return base().isDeadlock( s ); }
    bool isGoal( Node s ) { return base().isGoal( s ); }
    bool isAccepting( Node s ) { return base().isAccepting( s ); }
    std::string showNode( Node s ) { return base().showNode( s ); }
    void read( std::string path ) { base().read( path ); }
    void setDomainSize( const unsigned mpiRank = 0, const unsigned mpiSize = 1,
                        const unsigned peersCount = 1 ) {
        base().setDomainSize( mpiRank, mpiSize, peersCount );
    }

    PropertyType propertyType() { return base().propertyType(); }

    bool isInAccepting( Node s, const size_int_t acc_group ) {
        return base().isInAccepting( s, acc_group ); }
    bool isInRejecting( Node s, const size_int_t acc_group ) {
        return base().isInRejecting( s, acc_group ); }
    unsigned acceptingGroupCount() { return base().acceptingGroupCount(); }

    template< typename Q >
    void queueInitials( Q &q ) {
        base().queueInitials( q );
    }

    int setSlack( int s ) { return base().setSlack( s ); }

    template< typename O >
    O getProperties( O o ) {
        return base().getProperties( o );
    }

    /// Returns an owner id of the state n
    template< typename Hash, typename Worker >
    int owner( Hash &hash, Worker &worker, Node n, hash_t hint = 0 ) {
        return base().owner( hash, worker, n, hint );
    }

    /// Returns a position between 1 and n
    template< typename Alg >
    int successorNum( Alg &a, Node current, Node next )
    {
        typename G::Successors succ = base().successors( current );
        int edge = 0;
        while ( !succ.empty() ) {
            ++ edge;
            if ( a.equal( succ.head(), next ) )
                break;
            base().release( succ.head() ); // not it
            succ = succ.tail();
            assert( !succ.empty() ); // we'd hope the node is actually there!
        }

        while ( !succ.empty() ) {
            base().release( succ.head() );
            succ = succ.tail();
        }

        assert_leq( 1, edge );
        return edge;
    }
};

}
}

#endif
