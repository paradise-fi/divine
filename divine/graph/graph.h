// -*- C++ -*- (c) 2011 Petr Rockai <me@mornfall.net>

#include <divine/graph/allocator.h>
#include <divine/utility/meta.h>

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

    void useProperty( meta::Input & ) {}

    /// Makes a nonpermanent copy of a state
    Node copyState( Node n ) {
        Node copy( pool(), n.size() );
        n.copyTo( copy );
        copy.header().permanent = false;
        return copy;
    }

    /// Makes a duplicate that can be released (permanent states are not duplicated)
    Node clone( Node n ) {
        assert( n.valid() );
        return n.header().permanent ? n : copyState( n );
    }

    /// Returns an owner id of the state n
    template< typename Hash, typename Worker >
    int owner( Hash &hasher, Worker &worker, Node n, hash_t hint = 0 ) {
        // if n is not valid, this correctly returns owner for hint, because hash( n ) is 0
        if ( !hint )
            return hasher.hash( n ) % worker.peers();
        else
            return hint % worker.peers();
    }
};

template< typename G >
struct Transform {
    G _base;

    typedef wibble::Unit HasAllocator;
    typedef typename G::Node Node;
    typedef G Graph;

    G &base() { return _base; }
    Pool &pool() { return base().pool(); }

    Node initial() { return base().initial(); } // XXX remove

    template< typename Yield >
    void successors( Node st, Yield yield ) { base().successors( st, yield ); }
    void release( Node s ) { base().release( s ); }
    bool isDeadlock( Node s ) { return base().isDeadlock( s ); }
    bool isGoal( Node s ) { return base().isGoal( s ); }
    bool isAccepting( Node s ) { return base().isAccepting( s ); }
    std::string showNode( Node s ) { return base().showNode( s ); }
    std::string showTransition( Node from, Node to ) { return base().showTransition( from, to ); }
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
    Node clone( Node n ) { return base().clone( n ); }

    template< typename O >
    O getProperties( O o ) {
        return base().getProperties( o );
    }

    void useProperty( meta::Input &n ) {
        base().useProperty( n );
    }

    /// Returns an owner id of the state n
    template< typename Hash, typename Worker >
    int owner( Hash &hash, Worker &worker, Node n, hash_t hint = 0 ) {
        return base().owner( hash, worker, n, hint );
    }

    /// Returns a position between 1 and n
    template< typename Alg >
    int successorNum( Alg &a, Node current, Node next, int fromIndex = 0 )
    {
        int edge = 0;
        bool found = false;

        base().successors( current, [&]( Node n ) {
                if (!found)
                    ++ edge;
                if ( edge > fromIndex && a.store().equal( n, next ) )
                    found = true;
                this->base().release( n );
            } );

        assert_leq( 1, edge );
        assert( found );
        return edge;
    }

};

}
}

#endif
