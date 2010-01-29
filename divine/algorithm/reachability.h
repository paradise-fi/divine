// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h>

#include <divine/ltlce.h>
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
        if( f == &Reachability< G >::_parentTrace )
            return 1;
        assert_die();
    }

    static void (Reachability< G >::*from_id( int n ))()
    {
        switch ( n ) {
            case 0: return &Reachability< G >::_visit;
            case 1: return &Reachability< G >::_parentTrace;
            default: assert_die();
        }
    }

    template< typename O >
    static void writeShared( typename Reachability< G >::Shared s, O o ) {
        *o++ = s.initialTable;
        o = s.stats.write( o );
        *o++ = s.goal.valid();
        if ( s.goal.valid() )
            o = s.goal.write32( o );
    }

    template< typename I >
    static I readShared( typename Reachability< G >::Shared &s, I i ) {
        s.initialTable = *i++;
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

/**
 * A simple parallel reachability analysis implementation. Nothing to worry
 * about here.
 */
template< typename G >
struct Reachability : Algorithm, DomainWorker< Reachability< G > >
{
    typedef typename G::Node Node;

    struct Shared {
        Node goal;
        Statistics< G > stats;
        G g;
        CeShared< Node > ce;
        int initialTable;
    } shared;

    Domain< Reachability< G > > &domain() {
        return DomainWorker< Reachability< G > >::domain();
    }

    struct Extension {
        Blob parent;
    };

    LtlCE< G, Shared, Extension > ce;

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
            shared.stats.goals ++;
            shared.goal = t;
            return visitor::TerminateOnTransition;
        }

        return visitor::FollowTransition;
    }

    void _visit() { // parallel
        typedef visitor::Setup< G, Reachability< G >, Table > VisitorSetup;

        m_initialTable = &shared.initialTable;
        visitor::Parallel< VisitorSetup, Reachability< G >, Hasher >
            vis( shared.g, *this, *this, hasher, &table() );
        vis.exploreFrom( shared.g.initial() );
    }

    Reachability( Config *c = 0 )
        : Algorithm( c, sizeof( Extension ) )
    {
        initGraph( shared.g );
        if ( c ) {
            becomeMaster( &shared, workerCount( c ) );
            shared.initialTable = c->initialTableSize();
        }
    }

    void _parentTrace() {
        ce.setup( shared.g, shared ); // XXX this will be done many times needlessly
        ce._parentTrace( *this, hasher, equal, table() );
    }

    void counterexample( Node n ) {
        shared.ce.initial = n;
        ce.setup( shared.g, shared );
        ce.linear( domain(), *this );
    }

    Result run() {
        std::cerr << "  searching... \t" << std::flush;

        domain().parallel().run( shared, &Reachability< G >::_visit );

        for ( int i = 0; i < domain().peers(); ++i )
            shared.stats.merge( domain().shared( i ).stats );

        std::cerr << shared.stats.states << " states, "
                  << shared.stats.transitions << " edges" << std::endl;

        Node goal;

        for ( int i = 0; i < domain().peers(); ++i ) {
            Shared &s = domain().shared( i );
            if ( s.goal.valid() ) {
                goal = s.goal;
                break;
            }
        }

        safetyBanner( !goal.valid() );
        if ( goal.valid() )
            counterexample( goal );

        result().fullyExplored = Result::Yes;
        shared.stats.updateResult( result() );
        return result();
    }
};

}
}

#endif
