// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h>

#include <divine/graph/ltlce.h>

#ifndef DIVINE_ALGORITHM_REACHABILITY_H
#define DIVINE_ALGORITHM_REACHABILITY_H

namespace divine {
namespace algorithm {

template < typename VertexId >
struct ReachabilityShared {
    VertexId goal;
    bool deadlocked;
    algorithm::Statistics stats;
    CeShared< Blob, VertexId > ce;
    bool need_expand;
    ReachabilityShared() : need_expand( false ) {}
};

template< typename BS, typename VertexId >
typename BS::bitstream &operator<<( BS &bs, ReachabilityShared< VertexId > st )
{
    return bs << st.goal << st.deadlocked << st.stats << st.ce << st.need_expand;
}

template< typename BS, typename VertexId >
typename BS::bitstream &operator>>( BS &bs, ReachabilityShared< VertexId > &st )
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
    ALGORITHM_CLASS( Setup, ReachabilityShared< typename Setup::VertexId> );

    VertexId goal;
    bool deadlocked;

    struct Extension {
        VertexId parent;
    };

    typedef LtlCE< Setup, Shared, Extension, typename Store::Hasher > CE;
    CE ce;

    Pool& pool() {
        return this->graph().pool();
    }

    Extension &extension( Vertex n ) {
        return n.getVertexId().template extension< Extension >( pool() );
    }

    struct Main : Visit< This, Setup >
    {
        static visitor::ExpansionAction expansion( This &r, Vertex st )
        {
            r.shared.stats.addNode( r.graph(), st.getNode() );
            return visitor::ExpansionAction::Expand;
        }

        static visitor::TransitionAction transition( This &r, Vertex f, Vertex t, Label )
        {
            if ( !r.store().valid( r.extension( t ).parent ) ) {
                r.extension( t ).parent = f.getVertexId();
                assert( !r.store().valid( f )
                        || r.store().valid( r.extension( t ).parent ) );
            }
            r.shared.stats.addEdge( r.graph(), f.getNode(), t.getNode() );

            if ( r.meta().input.propertyType == graph::PT_Goal
                    && r.graph().isGoal( t.getNode() ) )
            {
                r.shared.goal = t.getVertexId();
                assert( r.store().valid( r.shared.goal ) );
                r.shared.deadlocked = false;
                return visitor::TransitionAction::Terminate;
            }

            r.graph().porTransition( f, t, 0 );
            return visitor::TransitionAction::Follow;
        }

        static visitor::DeadlockAction deadlocked( This &r, Vertex n )
        {
            r.shared.stats.addDeadlock();

            if ( r.meta().input.propertyType != graph::PT_Deadlock )
                return visitor::DeadlockAction::Ignore;

            r.shared.goal = n.getVertexId();
            assert( r.store().valid( r.shared.goal ) );
            r.shared.deadlocked = true;
            return visitor::DeadlockAction::Terminate;
        }
    };

    void _visit() { // parallel
        this->visit( this, Main(), shared.need_expand );
    }

    void _por_worker() {
        this->graph()._porEliminate( *this, nullptr );
    }

    Shared _por( Shared sh ) {
        shared = sh;
        if ( this->graph().porEliminate( *this, *this ) )
            shared.need_expand = true;
        return shared;
    }

    Reachability( Meta m ) : Algorithm( m, sizeof( Extension ) )
    {
        this->init( this );
        this->becomeMaster( m.execution.threads, this );
    }

    Reachability( Reachability *master ) : Algorithm( master->meta(), sizeof( Extension ) )
    {
        this->init( this, master );
    }

    Shared runCe( Shared sh, void (CE::*ceCall)( This&, typename Setup::Store& ) ) {
        shared = sh;
        ce.setup( this->graph(), shared, this->store().hasher() );
        (ce.*ceCall)( *this, this->store() );
        return shared;
    }

    Shared _parentTrace( Shared sh ) {
        return runCe( sh, &CE::_parentTrace );
    }

    Shared _successorTrace( Shared sh ) {
        return runCe( sh, &CE::_successorTrace );
    }

    Shared _ceIsInitial( Shared sh ) {
        return runCe( sh, &CE::_ceIsInitial );
    }

    void counterexample( VertexId n ) {
        shared.ce.initial = n;
        ce.setup( this->graph(), shared, this->store().hasher() );
        Node goal = ce.linear( *this, *this );
        ce.goal( *this, goal );
    }

    void collect() {
        deadlocked = false;
        for ( int i = 0; i < int( shareds.size() ); ++i ) {
            shareds[ i ].stats.update( meta().statistics );
            if ( this->store().valid( shareds[ i ].goal ) ) {
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

        while ( !this->store().valid( goal ) ) {
            shared.need_expand = false;
            ring( &This::_por );

            if ( shared.need_expand ) {
                parallel( &This::_visit );
                collect();
            } else
                break;
        }

        progress() << meta().statistics.visited << " states, "
                   << meta().statistics.transitions << " edges" << std::flush;

        if ( this->store().valid( goal ) ) {
            if ( deadlocked )
                progress() << ", DEADLOCK";
            else
                progress() << ", GOAL";
        }
        progress() << std::endl;

        resultBanner( !this->store().valid( goal ) );
        if ( this->store().valid( goal ) && this->meta().output.wantCe ) {
            counterexample( goal );
            result().ceType = deadlocked ? meta::Result::Deadlock : meta::Result::Goal;
        }

        result().propertyHolds = this->store().valid( goal )
                                  ? meta::Result::No : meta::Result::Yes;
        result().fullyExplored = this->store().valid( goal )
                                  ? meta::Result::No : meta::Result::Yes;
    }
};


ALGORITHM_RPC( Reachability );
ALGORITHM_RPC_ID( Reachability, 1, _visit );
ALGORITHM_RPC_ID( Reachability, 2, _parentTrace );
ALGORITHM_RPC_ID( Reachability, 3, _por );
ALGORITHM_RPC_ID( Reachability, 4, _por_worker );
ALGORITHM_RPC_ID( Reachability, 5, _successorTrace );
ALGORITHM_RPC_ID( Reachability, 6, _ceIsInitial );

}
}

#endif
