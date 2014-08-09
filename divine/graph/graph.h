// -*- C++ -*- (c) 2011-2014 Petr Rockai <me@mornfall.net>

#include <vector>
#include <divine/toolkit/pool.h>
#include <divine/toolkit/bitoperations.h>

#ifndef DIVINE_GRAPH_H
#define DIVINE_GRAPH_H

namespace divine {

namespace graph {

/// Types of acceptance condition
enum PropertyType { PT_None, PT_Goal, PT_Deadlock, PT_Buchi, PT_GenBuchi, PT_Muller, PT_Rabin, PT_Streett };
enum ReductionType { R_POR, R_Tau, R_TauPlus, R_TauStores, R_Heap, R_LU };
typedef std::set< ReductionType > ReductionSet;
typedef std::set< std::string > PropertySet;

enum class DemangleStyle { None, Cpp };

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
    bool isAccepting( Node ) { return false; }

    // for multi-set acceptance conditions (Rabin, Streett, ...)
    bool isInAccepting( Node, int /* acc_group */ ) { return false; }
    bool isInRejecting( Node, int /* acc_group */ ) { return false; }
    unsigned acceptingGroupCount() { return 0; }

	// HACK: Inform the gaph if fairness is enabled,
	// The timed automata interpreter uses this to enable Zeno reduction
	void fairnessEnabled( bool ) {}

    void demangleStyle( DemangleStyle ) { }

    template< typename Y >
    void properties( Y yield ) {
        yield( "deadlock", "(deadlock freedom)", PT_Deadlock );
    }

    void useProperties( PropertySet ) {}

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
    int owner( Hash &hasher, Worker &worker, Node n, hash64_t hint = 0 ) {
        // if n is not valid, this correctly returns owner for hint, because hash( n ) is 0
        if ( !hint )
            return hasher.hash( n ) % worker.peers();
        else
            return hint % worker.peers();
    }

    static const int SPLIT_LIMIT = 32; // should be multiple of 8

    template< typename Coroutine >
    void splitHint( Coroutine &cor, int a = 0, int b = 0, int chunk = SPLIT_LIMIT ) {
        if ( !a )
            a = cor.unconsumed();

        for ( int length : { a, b } ) {
            if ( !length )
                continue;

            if ( length <= chunk ) {
                cor.consume( length );
                continue;
            }

            unsigned count = align( length, chunk ) / chunk;
            assert_leq( int( (count - 1) * chunk ), length );
            assert_leq( length, int( count * chunk ) );

            unsigned msb = bitops::onlyMSB( count );
            unsigned nomsb = count & ~msb;
            unsigned rem = count * chunk - length;
            assert_leq( 0, int( rem ) );
            assert_leq( int( rem ), chunk );

            if ( !nomsb ) {
                msb /= 2;
                nomsb = msb;
            }

            assert_eq( msb + nomsb, count );
            cor.recurse( 2, msb * chunk, nomsb * chunk - rem, chunk );
        }
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

	void fairnessEnabled( bool enabled ) {
		base().fairnessEnabled( enabled );
	}

    void demangleStyle( DemangleStyle st ) {
        base().demangleStyle( st );
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

    void useProperties( PropertySet n ) {
        base().useProperties( n );
    }

    ReductionSet useReductions( ReductionSet r ) {
        return base().useReductions( r );
    }

    /// Returns an owner id of the state n
    template< typename Hash, typename Worker >
    int owner( Hash &hash, Worker &worker, Node n, hash64_t hint = 0 ) {
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
