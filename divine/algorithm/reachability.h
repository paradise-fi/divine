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

template< typename, typename > struct Reachability;

// MPI function-to-number-and-back-again drudgery... To be automated.
template< typename G, typename S >
struct _MpiId< Reachability< G, S > >
{
    typedef Reachability< G, S > A;

    static int to_id( void (A::*f)() ) {
        if( f == &A::_visit )
            return 0;
        if( f == &A::_parentTrace )
            return 1;
        assert_die();
    }

    static void (A::*from_id( int n ))()
    {
        switch ( n ) {
            case 0: return &A::_visit;
            case 1: return &A::_parentTrace;
            default: assert_die();
        }
    }

    template< typename O >
    static void writeShared( typename A::Shared s, O o ) {
        o = s.stats.write( o );
        *o++ = s.initialTable;
        *o++ = s.goal.valid();
        if ( s.goal.valid() )
            o = s.goal.write32( o );
    }

    template< typename I >
    static I readShared( typename A::Shared &s, I i ) {
        i = s.stats.read( i );
        s.initialTable = *i++;
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
template< typename G, typename Statistics >
struct Reachability : Algorithm, DomainWorker< Reachability< G, Statistics > >
{
    typedef Reachability< G, Statistics > This;
    typedef typename G::Node Node;

    struct Shared {
        Node goal;
        algorithm::Statistics< G > stats;
        G g;
        CeShared< Node > ce;
        int initialTable;
    } shared;

    Domain< This > &domain() {
        return DomainWorker< This >::domain();
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
            shared.goal = t;
            return visitor::TerminateOnTransition;
        }

        return visitor::FollowTransition;
    }

    struct VisitorSetup : visitor::Setup< G, This, Table, Statistics > {
        static visitor::DeadlockAction deadlocked( This &r, Node n ) {
            r.shared.goal = n;
            r.shared.stats.addDeadlock();
            return visitor::TerminateOnDeadlock;
        }
    };

    void _visit() { // parallel
        m_initialTable = &shared.initialTable;
        visitor::Parallel< VisitorSetup, This, Hasher >
            visitor( shared.g, *this, *this, hasher, &table() );
        shared.g.queueInitials( visitor );
        visitor.processQueue();
    }

    Reachability( Config *c = 0 )
        : Algorithm( c, sizeof( Extension ) )
    {
        initGraph( shared.g );
        if ( c ) {
            this->becomeMaster( &shared, workerCount( c ) );
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

        domain().parallel().run( shared, &This::_visit );

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
