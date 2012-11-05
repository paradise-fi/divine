// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h>

#include <divine/graph/ltlce.h>

#ifndef DIVINE_ALGORITHM_REACHABILITY_H
#define DIVINE_ALGORITHM_REACHABILITY_H

namespace divine {
namespace algorithm {

struct ReachabilityShared {
    Blob goal;
    bool deadlocked;
    algorithm::Statistics stats;
    CeShared< Blob > ce;
    bool need_expand;
};

template< typename BS >
typename BS::bitstream &operator<<( BS &bs, ReachabilityShared st )
{
    return bs << st.goal << st.deadlocked << st.stats << st.ce << st.need_expand;
}

template< typename BS >
typename BS::bitstream &operator>>( BS &bs, ReachabilityShared &st )
{
    return bs >> st.goal >> st.deadlocked >> st.stats >> st.ce >> st.need_expand;
}

/**
 * A simple parallel reachability analysis implementation. Nothing to worry
 * about here.
 */
template< typename Setup >
struct Reachability : Algorithm, AlgorithmUtils< Setup >,
                      Parallel< Setup::template Topology, Reachability< Setup > >
{
    typedef Reachability< Setup > This;
    ALGORITHM_CLASS( Setup, ReachabilityShared );

    Node goal;
    bool deadlocked;

    struct Extension {
        Blob parent;
    };

    LtlCE< Graph, Shared, Extension > ce;

    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    struct Main : Visit< This, Setup >
    {
        static visitor::ExpansionAction expansion( This &r, Node st )
        {
            r.shared.stats.addNode( r.graph(), st );
            r.graph().porExpansion( st );
            return visitor::ExpandState;
        }

        static visitor::TransitionAction transition( This &r, Node f, Node t, Label )
        {
            if ( !r.extension( t ).parent.valid() ) {
                r.extension( t ).parent = f;
                visitor::setPermanent( f );
            }
            r.shared.stats.addEdge( r.graph(), f, t );

            if ( r.meta().algorithm.findGoals && r.graph().isGoal( t ) ) {
                r.shared.goal = r.graph().clone( t );
                r.shared.deadlocked = false;
                return visitor::TerminateOnTransition;
            }

            r.graph().porTransition( f, t, 0 );
            return visitor::FollowTransition;
        }

        static visitor::DeadlockAction deadlocked( This &r, Node n )
        {
            if ( !r.meta().algorithm.findDeadlocks )
                return visitor::IgnoreDeadlock;

            r.shared.goal = r.graph().clone( n );
            r.shared.stats.addDeadlock();
            r.shared.deadlocked = true;
            return visitor::TerminateOnDeadlock;
        }
    };

    void _visit() { // parallel
        this->visit( this, Main() );
    }

    void _por_worker() {
        this->graph()._porEliminate( *this );
    }

    Shared _por( Shared sh ) {
        shared = sh;
        if ( this->graph().porEliminate( *this, *this ) )
            shared.need_expand = true;
        return shared;
    }

    Reachability( Meta m, bool master = false )
        : Algorithm( m, sizeof( Extension ) )
    {
        if ( master )
            this->becomeMaster( m.execution.threads, m );
        this->init( this );
    }

    Shared _parentTrace( Shared sh ) {
        shared = sh;
        ce.setup( this->graph(), shared ); // XXX this will be done many times needlessly
        ce._parentTrace( *this, this->store() );
        return shared;
    }

    void counterexample( Node n ) {
        shared.ce.initial = n;
        ce.setup( this->graph(), shared );
        ce.linear( *this, *this );
        ce.goal( *this, n );
    }

    void collect() {
        deadlocked = false;
        for ( int i = 0; i < int( shareds.size() ); ++i ) {
            shared.stats.merge( shareds[ i ].stats );
            if ( shareds[ i ].goal.valid() ) {
                goal = shareds[ i ].goal;
                if ( shareds[ i ].deadlocked )
                    deadlocked = true;
            }
        }
    }

    void run() {
        progress() << "  searching... \t" << std::flush;

        parallel( &This::_visit );
        collect();

        while ( !goal.valid() ) {
            shared.need_expand = false;
            ring( &This::_por );

            if ( shared.need_expand ) {
                parallel( &This::_visit );
                collect();
            } else
                break;
        }

        progress() << shared.stats.states << " states, "
                   << shared.stats.transitions << " edges" << std::flush;

        if ( goal.valid() ) {
            if ( deadlocked )
                progress() << ", DEADLOCK";
            else
                progress() << ", GOAL";
        }
        progress() << std::endl;

        safetyBanner( !goal.valid() );
        if ( goal.valid() ) {
            counterexample( goal );
            result().ceType = deadlocked ? meta::Result::Deadlock : meta::Result::Goal;
        }

        meta().input.propertyType = meta::Input::Reachability;
        result().propertyHolds = goal.valid() ? meta::Result::No : meta::Result::Yes;
        result().fullyExplored = goal.valid() ? meta::Result::No : meta::Result::Yes;
        shared.stats.update( meta().statistics );
    }
};


ALGORITHM_RPC( Reachability );
ALGORITHM_RPC_ID( Reachability, 1, _visit );
ALGORITHM_RPC_ID( Reachability, 2, _parentTrace );
ALGORITHM_RPC_ID( Reachability, 3, _por );
ALGORITHM_RPC_ID( Reachability, 4, _por_worker );

}
}

#endif
