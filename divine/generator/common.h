// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/stateallocator.h>
#include <divine/blob.h>
#include <divine/hashset.h>
#include <divine/algorithm/common.h>

#ifndef DIVINE_GENERATOR_COMMON_H
#define DIVINE_GENERATOR_COMMON_H

namespace divine {
namespace generator {

struct Allocator : StateAllocator {
    Pool _pool;
    int _slack;

    Allocator() : _slack( 0 ) {}

    void setSlack( int s ) { _slack = s; }
    Pool &pool() { return _pool; }

    virtual state_t legacy_state( Blob b ) {
        divine::state_t s;
        s.size = b.size() - _slack;
        s.ptr = b.data() + _slack;
        return s;
    }

    virtual Blob unlegacy_state( state_t s ) {
        return Blob( s.ptr - _slack, true );
    }

    state_t duplicate_state( const state_t &st ) {
        Blob a = unlegacy_state( st ), b( pool(), st.size + _slack );
        a.copyTo( b );
        if ( _slack )
            b.clear( 0, _slack );
        return legacy_state( b );
    }

    state_t new_state( const std::size_t size ) {
        Blob b( pool(), size + _slack );
        b.clear();
        return legacy_state( b );
    }

    Blob new_blob( std::size_t size ) {
        Blob b = Blob( pool(), size + _slack );
        b.clear();
        return b;
    }

    void delete_state( state_t &st ) {
        Blob a( st.ptr, true );
        a.free( pool() );
    }
};

/// Types of acceptance condition
enum PropertyType { AC_None, AC_Buchi, AC_GenBuchi, AC_Muller, AC_Rabin, AC_Streett };

/// Superclass of all graph generators
template< typename _Node >
struct Common {
    typedef _Node Node;

    Allocator alloc;
    int setSlack( int s ) { alloc.setSlack( s ); return s; }
    Pool &pool() { return alloc.pool(); }
    void initPOR() {}

    /// Acceptance condition type. This should be overriden in subclasses.
    PropertyType propertyType() { return AC_None; }

    /// Is state s in accepting set? This should be overriden in subclasses.
    bool isAccepting( Node s ) { return false; }

    /// Is state s in accepting set? (non-buchi acceptance condition)
    bool isInAccepting( Node s, const size_int_t acc_group ) { return false; }

    /// Is state s in rejecting set? (non-buchi acceptance condition)
    bool isInRejecting( Node s, const size_int_t acc_group ) { return false; }

    /// Number of sets/pairs in acceptance condition. (non-buchi acceptance condition)
    unsigned acceptingGroupCount() { return 0; }

    /// Sets information about domain geometry
    void setDomainSize( const unsigned mpiRank = 0, const unsigned mpiSize = 1,
                        const unsigned peersCount = 1 ) {}

    /// Returns an owner id of the state n
    template< typename Hash, typename Worker >
    int owner( Hash &hash, Worker &worker, Node n, hash_t hint = 0 ) {
        assert( n.valid() );

        if ( !hint )
            return hash( n ) % worker.peers();
        else
            return hint % worker.peers();
    }

    /// Default storage for visited states, can be overriden by the generator
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
