// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h>

#include <divine/ltlce.h>
#include <divine/visitor.h>
#include <divine/parallel.h>

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
template< typename G, template< typename > class Topology, typename Statistics >
struct Reachability : Algorithm, AlgorithmUtils< G >,
                      Parallel< Topology, Reachability< G, Topology, Statistics > >
{
    typedef Reachability< G, Topology, Statistics > This;
    ALGORITHM_CLASS( G, ReachabilityShared );

    Node goal;
    bool deadlocked;

    struct Extension {
        Blob parent;
    };

    LtlCE< G, Shared, Extension > ce;

    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    visitor::ExpansionAction expansion( Node st )
    {
        shared.stats.addNode( m_graph, st );
        m_graph.porExpansion( st );
        return visitor::ExpandState;
    }

    visitor::TransitionAction transition( Node f, Node t )
    {
        if ( !meta().algorithm.hashCompaction ) {
            if ( !extension( t ).parent.valid() ) {
                extension( t ).parent = f;
                visitor::setPermanent( f );
            }
        } else {
            if ( !extension( t ).parent.ptr && f.valid() )
                extension( t ).parent.ptr = (char *)(uintptr_t) hasher( f );
        }
        shared.stats.addEdge();

        if ( meta().algorithm.findGoals && m_graph.isGoal( t ) ) {
            shared.goal = m_graph.clone( t );
            shared.deadlocked = false;
            return visitor::TerminateOnTransition;
        }

        m_graph.porTransition( f, t, 0 );
        return visitor::FollowTransition;
    }

    struct VisitorSetup : visitor::Setup< G, This, typename AlgorithmUtils< G >::Table, Statistics > {
        static visitor::DeadlockAction deadlocked( This &r, Node n ) {
            if ( !r.meta().algorithm.findDeadlocks )
                return visitor::IgnoreDeadlock;

            r.shared.goal = r.m_graph.clone( n );
            r.shared.stats.addDeadlock();
            r.shared.deadlocked = true;
            return visitor::TerminateOnDeadlock;
        }
    };

    void _visit() { // parallel
        this->comms().notify( this->id(), &m_graph.pool() );
        visitor::Partitioned< VisitorSetup, This, Hasher >
            visitor( m_graph, *this, *this, hasher, &this->table(), meta().algorithm.hashCompaction );
        m_graph.queueInitials( visitor );
        visitor.processQueue();
    }

    void _por_worker() {
        m_graph._porEliminate( *this, hasher, this->table() );
    }

    Shared _por( Shared sh ) {
        shared = sh;
        if ( m_graph.porEliminate( *this, *this ) )
            shared.need_expand = true;
        return shared;
    }

    Reachability( Meta m, bool master = false )
        : Algorithm( m, sizeof( Extension ) )
    {
        if ( master )
            this->becomeMaster( m.execution.threads, m );
        equal.allEqual = m.algorithm.hashCompaction; // has to be set before makeTable
        this->init( this );
    }

    Shared _parentTrace( Shared sh ) {
        shared = sh;
        ce.setup( m_graph, shared ); // XXX this will be done many times needlessly
        ce._parentTrace( *this, hasher, equal, this->table() );
        return shared;
    }

    Shared _hashTrace( Shared sh ) {
        shared = sh;
        ce.setup( m_graph, shared ); // XXX this will be done many times needlessly
        ce._hashTrace( *this, hasher, equal, this->table() );
        return shared;
    }

    void counterexample( Node n ) {
        shared.ce.initial = n;
        ce.setup( m_graph, shared );
        if ( !meta().algorithm.hashCompaction ) {
            ce.linear( *this, *this );
        } else {
            ce.linear_hc( *this, *this );
        }
        ce.goal( *this, n );
    }

    void collect() {
        deadlocked = false;
        for ( int i = 0; i < shareds.size(); ++i ) {
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
ALGORITHM_RPC_ID( Reachability, 5, _hashTrace );

}
}

#endif
