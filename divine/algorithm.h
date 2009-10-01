// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/config.h>
#include <divine/blob.h>
#include <divine/hashmap.h>
#include <divine/visitor.h>

#ifndef DIVINE_ALGORITHM_H
#define DIVINE_ALGORITHM_H

namespace divine {
namespace algorithm {

template< typename T >
struct _MpiId
{
    static int to_id( void (T::*f)() ) {
        // assert( 0 );
        return -1;
    }

    static void (T::*from_id( int ))() {
        // assert( 0 );
        return 0;
    }

    template< typename O >
    static void writeShared( typename T::Shared, O ) {
    }

    template< typename I >
    static I readShared( typename T::Shared &, I i ) {
        return i;
    }
};

inline int workerCount( Config *c ) {
    if ( !c )
        return 1;
    return c->workers();
}

struct Hasher {
    int slack;

    Hasher( int s = 0 ) : slack( s ) {}
    void setSlack( int s ) { slack = s; }

    inline hash_t operator()( Blob b ) const {
        assert( b.valid() );
        return b.hash( slack, b.size() );
    }
};

struct Equal {
    int slack;
    Equal( int s = 0 ) : slack( s ) {}
    void setSlack( int s ) { slack = s; }
    inline hash_t operator()( Blob a, Blob b ) const {
        assert( a.valid() );
        assert( b.valid() );
        return a.compare( b, slack, std::max( a.size(), b.size() ) ) == 0;
    }
};

struct Algorithm
{
    typedef Blob Node; // Umm.

    typedef HashMap< Node, Unit, Hasher,
                     divine::valid< Node >, Equal > Table;

    Config *m_config;
    int m_slack;
    Hasher hasher;
    Equal equal;
    Table *m_table;

    bool want_ce;

    Config &config() {
        assert( m_config );
        return *m_config;
    }

    Table &table() {
        if ( !m_table )
            m_table = new Table( hasher, divine::valid< Node >(), equal );
        return *m_table;
    }

    void livenessBanner( bool valid ) {
        std::cerr << " ===================================== " << std::endl
                  << ( valid ?
                     "       Accepting cycle NOT found       " :
                     "         Accepting cycle FOUND         " )
                  << std::endl
                  << " ===================================== " << std::endl;
    }

    void safetyBanner( bool valid ) {
        std::cerr << " ===================================== " << std::endl
                  << ( valid ?
                     "          Goal state NOT found         " :
                     "            Goal state FOUND           " )
                  << std::endl
                  << " ===================================== " << std::endl;
    }

    template< typename G >
    void initGraph( G &g ) {
        g.setSlack( m_slack );
        if ( m_config ) { // this is the master instance
            g.read( m_config->input() );
        }
    }

    Algorithm( Config *c = 0, int slack = 0 )
        : m_config( c ), m_slack( slack ), m_table( 0 )
    {
        hasher.setSlack( slack );
        equal.setSlack( slack );
        if ( c ) {
            want_ce = c->generateCounterexample();
        }
    }
};

template< typename Node >
struct CeShared {
    Node initial;
    Node current;
    bool current_updated;
    template< typename I >
    I read( I i ) {
        FakePool fp;

        if ( *i++ )
            i = initial.read32( &fp, i );
        if ( *i++ )
            i = current.read32( &fp, i );
        current_updated = *i++;
        return i;
    }

    template< typename O >
    O write( O o ) {
        *o++ = initial.valid();
        if ( initial.valid() )
            o = initial.write32( o );
        *o++ = current.valid();
        if ( current.valid() )
            o = current.write32( o );
        *o++ = current_updated;
        return o;
    }
};

template< typename G, typename Shared, typename Extension >
struct LtlCE {
    typedef typename G::Node Node;
    typedef LtlCE< G, Shared, Extension > Us;

    G *_g;
    Shared *_shared;

    G &g() { assert( _g ); return *_g; }
    Shared &shared() { assert( _shared ); return *_shared; }

    LtlCE() : _g( 0 ), _shared( 0 ) {}

    // XXX duplicated from visitor
    template< typename Hash, typename Worker >
    int owner( Hash &hash, Worker &worker, Node n ) const {
        return hash( n ) % worker.peers();
    }

    void setup( G &g, Shared &s )
    {
        _g = &g;
        _shared = &s;
    }

    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    bool updateIteration( Node t ) {
        int old = extension( t ).iteration;
        extension( t ).iteration = shared().iteration;
        return old != shared().iteration;
    }

    // -------------------------------------
    // -- Parent trace extraction
    // --

    template< typename Worker, typename Hasher, typename Equal, typename Table >
    void _parentTrace( Worker &w, Hasher &h, Equal &eq, Table &t ) {
        if ( shared().ce.current_updated )
            return;
        if ( owner( h, w, shared().ce.current ) == w.globalId() ) {
            Node n = t.get( shared().ce.current ).key;
            assert( n.valid() );
            shared().ce.current = extension( n ).parent;
            shared().ce.current_updated = true;
        }
    }

    // -------------------------------------
    // -- Local cycle discovery
    // --

    visitor::ExpansionAction cycleExpansion( Node n ) {
        return visitor::ExpandState;
    }

    visitor::TransitionAction cycleTransition( Node from, Node to ) {
        if ( from.valid() && to == shared().ce.initial ) {
            extension( to ).parent = from;
            return visitor::TerminateOnTransition;
        }
        if ( updateIteration( to ) ) {
            extension( to ).parent = from;
            return visitor::ExpandTransition;
        }
        return visitor::ForgetTransition;
    }

    template< typename Worker, typename Hasher, typename Table >
    void _traceCycle( Worker &w, Hasher &h, Table &t ) {
        typedef visitor::Setup< G, Us, Table,
            &Us::cycleTransition,
            &Us::cycleExpansion > Setup;
        typedef visitor::Parallel< Setup, Worker, Hasher > Visitor;

        Visitor visitor( g(), w, *this, h, &t );
        assert( shared().ce.initial.valid() );
        if ( visitor.owner( shared().ce.initial ) == w.globalId() ) {
            shared().ce.initial = t.get( shared().ce.initial ).key;
            visitor.queue( Blob(), shared().ce.initial );
        }
        visitor.processQueue();
    }

    // ------------------------------------------------
    // -- Lasso counterexample generation
    // --

    template< typename Domain, typename Alg >
    void parentTrace( Domain &d, Alg a, Node stop, bool cycle = 0 ) {
        std::ostream &out = a.config().ceStream();
        shared().ce.current = shared().ce.initial;
        do {
            shared().ce.current_updated = false;
            out << g().showNode( shared().ce.current ) << std::endl;
            d.parallel().runInRing( shared(), &Alg::_parentTrace );
            assert( shared().ce.current_updated );
        } while ( !a.equal( shared().ce.current, stop ) );
        if ( !cycle )
            out << g().showNode( shared().ce.current ) << std::endl;
    }

    template< typename Domain, typename Alg >
    void lasso( Domain &d, Alg a ) {
        std::ostream &out = a.config().ceStream();
        out << std::endl << "===== Trace to initial ====="
            << std::endl << std::endl;
        parentTrace( d, a, g().initial(), false );

        ++ shared().iteration;
        d.parallel().run( shared(), &Alg::_traceCycle );

        out << std::endl << "===== The cycle ====="
            << std::endl << std::endl;
        parentTrace( d, a, shared().ce.initial, true );
    }

};

}
}

#endif
