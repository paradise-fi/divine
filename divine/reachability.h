// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm.h>
#include <divine/metrics.h>
#include <divine/visitor.h>
#include <divine/parallel.h>
#include <divine/report.h>

#ifndef DIVINE_REACHABILITY_H
#define DIVINE_REACHABILITY_H

namespace divine {
namespace algorithm {

template< typename > struct Reachability;

// MPI function-to-number-and-back-again drudgery... To be automated.
template< typename G >
struct _MpiId< Reachability< G > >
{
    static int to_id( void (Reachability< G >::*f)() ) {
        if( f == &Reachability< G >::_visit )
            return 0;
        if( f == &Reachability< G >::_counterexample )
            return 1;
        assert_die();
    }

    static void (Reachability< G >::*from_id( int n ))()
    {
        switch ( n ) {
            case 0: return &Reachability< G >::_visit;
            case 1: return &Reachability< G >::_counterexample;
            default: assert_die();
        }
    }

    template< typename O >
    static void writeShared( typename Reachability< G >::Shared s, O o ) {
        o = s.stats.write( o );
        *o++ = s.goal.valid();
        if ( s.goal.valid() )
            o = s.goal.write32( o );
    }

    template< typename I >
    static I readShared( typename Reachability< G >::Shared &s, I i ) {
        i = s.stats.read( i );
        bool valid = *i++;
        if ( valid ) {
            FakePool fp;
            i = s.goal.read32( &fp, i );
        }
        return i;
    }
};
// END MPI drudgery

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

    Successors successors( Node n ) {
        Successors s;
        s._from = n;
        return s;
    }

    void release( Node ) {}

    ParentGraph( Node ini ) : _initial( ini ) {}
};

template< typename G >
struct Reachability : DomainWorker< Reachability< G > >
{
    typedef typename G::Node Node;

    struct Shared {
        Node goal;
        Statistics< G > stats;
        G g;
    } shared;

    Domain< Reachability< G > > m_domain;
    Domain< Reachability< G > > &domain() { return m_domain; }

    struct Extension {
        Blob parent;
    };

    typedef HashMap< Node, Unit, Hasher,
                     divine::valid< Node >, Equal > Table;
    Table *m_table;
    Hasher hasher;

    Table &table() {
        if ( !m_table ) {
            m_table = new Table( hasher, divine::valid< Node >(),
                                 Equal( hasher.slack ) );
        }
        return *m_table;
    }

    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    visitor::ExpansionAction expansion( Node st )
    {
        shared.stats.addNode( shared.g, st );
        return visitor::ExpandState;
    }

    visitor::TransitionAction transition( Node f, Node t )
    {
        if ( !extension( t ).parent.valid() ) {
            extension( t ).parent = f;
            visitor::setPermanent( f );
        }
        shared.stats.addEdge();

        if ( shared.g.isGoal( t ) ) {
            shared.goal = t;
            return visitor::TerminateOnTransition;
        }

        return visitor::FollowTransition;
    }

    void _visit() { // parallel
        typedef visitor::Setup< G, Reachability< G >, Table > VisitorSetup;

        visitor::Parallel< VisitorSetup, Reachability< G >, Hasher >
            vis( shared.g, *this, *this, hasher, &table() );
        vis.visit( shared.g.initial() );
    }

    Reachability( Config *c = 0 )
        : m_domain( &shared, workerCount( c ) ), m_table( 0 )
    {
        hasher.setSlack( sizeof( Extension ) );
        shared.g.setSlack( sizeof( Extension ) );
        if ( c )
            shared.g.read( c->input() );
    }

    visitor::ExpansionAction ceExpansion( Node n ) {
        std::cerr << shared.g.showNode( n ) << std::endl;
        return visitor::ExpandState;
    }

    visitor::TransitionAction ceTransition( Node f, Node t ) {
        return visitor::ExpandTransition;
    }

    void _counterexample() {
        typedef ParentGraph< G, Extension > PG;
        typedef visitor::Setup< PG, Reachability< G >, Table,
            &Reachability< G >::ceTransition,
            &Reachability< G >::ceExpansion > VisitorSetup;

        PG pg( shared.goal );
        visitor::Parallel< VisitorSetup, Reachability< G >, Hasher >
            vis( pg, *this, *this, hasher, &table() );
        if ( vis.owner( shared.goal ) == this->globalId() )
            vis.queue( Blob(), shared.goal );
        vis.visit();
    }

    void counterexample( Node n ) {
        std::cerr << std::endl << "===== GOAL ====="
                  << std::endl << std::endl
                  << shared.g.showNode( n ) << std::endl;

        shared.goal = n;

        std::cerr << std::endl << "===== Trace to initial ====="
                  << std::endl << std::endl;
        domain().parallel().run( shared, &Reachability< G >::_counterexample );
    }

    Result run() {
        domain().parallel().run( shared, &Reachability< G >::_visit );

        for ( int i = 0; i < domain().peers(); ++i )
            shared.stats.merge( domain().shared( i ).stats );

        for ( int i = 0; i < domain().peers(); ++i ) {
            Shared &s = domain().shared( i );
            if ( s.goal.valid() ) {
               counterexample( s.goal );
               break;
            }
        }

        shared.stats.print( std::cerr );

        Result res;
        res.fullyExplored = Result::Yes;
        shared.stats.updateResult( res );
        return res;
    }
};

}
}

#endif
