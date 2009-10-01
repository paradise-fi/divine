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

template< typename G, typename Ext >
struct ParentGraph {
    typedef typename G::Node Node;
    Node _initial;

    struct Successors {
        Node _from;
        bool empty() {
            if ( !_from.valid() )
                return true;
            if ( !_from.template get< Ext >().parent.valid() )
                return true;
            return false;
        }

        Node from() { return _from; }

        Successors tail() {
            Successors s;
            s._from = Blob();
            return s;
        }

        Node head() {
            return _from.template get< Ext >().parent;
        }
    };

    Node initial() {
        return _initial;
    }

    void setInitial( Node n ) { _initial = n; }

    Successors successors( Node n ) {
        Successors s;
        s._from = n;
        return s;
    }

    void release( Node ) {}

    ParentGraph( Node ini ) : _initial( ini ) {}
};

template< typename G, typename Shared, typename Extension >
struct LtlCE {
    typedef typename G::Node Node;
    typedef LtlCE< G, Shared, Extension > Us;

    G *_g;
    Shared *_shared;
    Node ce_node;

    G &g() { assert( _g ); return *_g; }
    Shared &shared() { assert( _shared ); return *_shared; }

    LtlCE() : _g( 0 ), _shared( 0 ) {}

    void setup( G &g, Shared &s )
    {
        _g = &g;
        _shared = &s;
    }

    void setFrom( Node n ) { ce_node = n; }

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

    visitor::ExpansionAction traceExpansion( Node n ) {
        std::cerr << g().showNode( n ) << std::endl;
        return visitor::ExpandState;
    }

    visitor::TransitionAction traceTransition( Node, Node to ) {
        return visitor::FollowTransition;
    }

    template< typename Worker, typename Hasher, typename Equal, typename Table >
    void _parentTrace( Worker &w, Hasher &h, Equal &eq, Table &t ) {
        typedef ParentGraph< G, Extension > PG;
        typedef visitor::Setup< PG, Us, Algorithm::Table,
            &Us::traceTransition,
            &Us::traceExpansion > VisitorSetup;

        Algorithm::Table table( h, divine::valid< Node >(), eq );
        assert( ce_node.valid() );
        PG pg( ce_node );
        visitor::Parallel< VisitorSetup, Worker, Hasher >
            vis( pg, w, *this, h, &table );

        if ( vis.owner( ce_node ) == w.globalId() ) {
            ce_node = t.get( ce_node ).key;
            assert( ce_node.valid() );
            pg.setInitial( ce_node );
            vis.queue( Blob(), ce_node );
        }
        vis.processQueue();
    }

    // -------------------------------------
    // -- Local cycle discovery
    // --

    visitor::ExpansionAction cycleExpansion( Node n ) {
        return visitor::ExpandState;
    }

    visitor::TransitionAction cycleTransition( Node from, Node to ) {
        if ( from.valid() && to == ce_node ) {
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
        assert( ce_node.valid() );
        if ( visitor.owner( ce_node ) == w.globalId() ) {
            ce_node = t.get( ce_node ).key;
            visitor.queue( Blob(), ce_node );
        }
        visitor.processQueue();
    }

    // ------------------------------------------------
    // -- Lasso counterexample generation
    // --

    template< typename Domain, typename Alg >
    void lasso( Domain &d, Alg ) {
        std::cerr << std::endl << "===== Trace to initial ====="
                  << std::endl << std::endl;
        ++ shared().iteration;
        d.parallel().run( shared(), &Alg::_parentTrace );

        ++ shared().iteration;
        d.parallel().run( shared(), &Alg::_traceCycle );

        std::cerr << std::endl << "===== The cycle ====="
                  << std::endl << std::endl;
        ++ shared().iteration;
        d.parallel().run( shared(), &Alg::_parentTrace );
    }

};

}
}

#endif
