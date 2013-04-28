// -*- C++ -*- (c) 2011 Petr Rockai <me@mornfall.net>

#include <vector>
#include <divine/toolkit/pool.h>

#ifndef DIVINE_GRAPH_H
#define DIVINE_GRAPH_H

namespace divine {

    enum class Recurse : uint8_t { Yes, No };

namespace graph {

/// Types of acceptance condition
enum PropertyType { PT_Goal, PT_Deadlock, PT_Buchi, PT_GenBuchi, PT_Muller, PT_Rabin, PT_Streett };
enum ReductionType { R_POR, R_Tau, R_TauPlus, R_TauStores, R_Heap, R_LU };
typedef std::set< ReductionType > ReductionSet;

struct Allocator {
    Pool _pool;
    int _slack;

    int setSlack( int s ) { return _slack = s; }
    Pool &pool() { return _pool; }
    void setPool( Pool p ) { _pool = p; }
    int slack() { assert_leq( 0, _slack ); return _slack; }

    Allocator() : _pool( nullptr ), _slack( 0 ) {}

    Blob makeBlob( int s ) {
        Blob b = pool().allocate( s + slack() );
        if ( slack() )
            pool().clear( b, 0, slack() );
        return b;
    }

    Blob makeBlobCleared( int s ) {
        Blob b = pool().allocate( s + slack() );
        pool().clear( b );
        return b;
    }
};

template< typename _Node >
struct Base : Allocator {
    typedef _Node Node;
    typedef wibble::Unit Label;

    void initPOR() {}

    // for single-set acceptance conditions (Buchi)
    bool isAccepting( Node s ) { return false; }

    // for multi-set acceptance conditions (Rabin, Streett, ...)
    bool isInAccepting( Node s, int acc_group ) { return false; }
    bool isInRejecting( Node s, int acc_group ) { return false; }
    unsigned acceptingGroupCount() { return 0; }

    // HACK: Inform the graph of the compute domain geometry, required by the
    // Compact generator for more efficient operation
    void setDomainSize( unsigned /* mpiRank */ = 0, unsigned /* mpiSize */ = 1,
                        unsigned /* peersCount */ = 1 ) {}

	// HACK: Inform the gaph if fairness is enabled,
	// The timed automata interpreter uses this to enable Zeno reduction
	void fairnessEnabled( bool ) {}

    template< typename Y >
    void properties( Y yield ) {
        yield( "deadlock", "(deadlock freedom)", PT_Deadlock );
    }

    void useProperty( std::string ) {}

    virtual ReductionSet useReductions( ReductionSet ) {
        return ReductionSet();
    }

    /// Makes a nonpermanent copy of a state
    Node copyState( Node n ) {
        Node copy = pool().allocate( pool().size( n ) );
        pool().copy( n, copy );
        return copy;
    }

    /// Makes a duplicate that can be released (permanent states are not duplicated)
    Node clone( Node n ) {
        assert( pool().valid( n ) );
        return copyState( n );
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

    // yield( Recurse, length, remaining count )
    template< typename Yield >
    void splitHint( Node n, int form, int length, Yield yield ) {
        // just a simple binary splitting, should be replaced by something
        // more appropriate in generators

        if ( length <= 32 ) {
            yield( Recurse::No, length, 0 );
            return;
        }

        int half = length / 2;
        half = half & 0x1 ? half + 1: half; // ensure even number

        yield( Recurse::Yes, half, 1 );
        yield( Recurse::Yes, length - half, 0 );
    }

    template< typename Yield >
    void splitHint( Node n, Yield yield ) {
        return splitHint( n, slack(), pool().size( n ) - slack(), yield );
    }

    std::string showConstdata() { return ""; }
};

template< typename G >
struct Transform {
    G _base;

    typedef wibble::Unit HasAllocator;
    typedef typename G::Node Node;
    typedef typename G::Label Label;
    typedef G Graph;

    G &base() { return _base; }
    Pool &pool() { return base().pool(); }
    int slack() { return base().slack(); }

    template< typename Yield >
    void successors( Node st, Yield yield ) { base().successors( st, yield ); }
    template< typename Yield >
    void allSuccessors( Node st, Yield yield ) { successors( st, yield ); }
    void release( Node s ) { base().release( s ); }
    bool isDeadlock( Node s ) { return base().isDeadlock( s ); }
    bool isGoal( Node s ) { return base().isGoal( s ); }
    bool isAccepting( Node s ) { return base().isAccepting( s ); }
    std::string showConstdata() { return base().showConstdata(); }
    std::string showNode( Node s ) { return base().showNode( s ); }
    std::string showTransition( Node from, Node to, Label act ) { return base().showTransition( from, to, act ); }
    void read( std::string path, std::vector< std::string > definitions, Transform< G > *blueprint = nullptr ) {
        base().read( path, definitions, blueprint ? &blueprint->_base : nullptr );
    }
    void setDomainSize( const unsigned mpiRank = 0, const unsigned mpiSize = 1,
                        const unsigned peersCount = 1 ) {
        base().setDomainSize( mpiRank, mpiSize, peersCount );
    }

	void fairnessEnabled( bool enabled ) {
		base().fairnessEnabled( enabled );
	}

    bool isInAccepting( Node s, int acc_group ) {
        return base().isInAccepting( s, acc_group ); }
    bool isInRejecting( Node s, int acc_group ) {
        return base().isInRejecting( s, acc_group ); }
    unsigned acceptingGroupCount() { return base().acceptingGroupCount(); }

    template< typename Yield >
    void initials( Yield yield ) {
        base().initials( yield );
    }

    void setPool( Pool p ) { base().setPool( p ); }
    int setSlack( int s ) { return base().setSlack( s ); }
    Node clone( Node n ) { return base().clone( n ); }

    template< typename Y >
    void properties( Y yield ) {
        return base().properties( yield );
    }

    void useProperty( std::string n ) {
        base().useProperty( n );
    }

    ReductionSet useReductions( ReductionSet r ) {
        return base().useReductions( r );
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
        if ( !pool().valid( current ) || !pool().valid( next ) )
            return 0;

        int edge = 0;
        bool found = false;

        base().successors( current, [&]( Node n, Label ) {
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
