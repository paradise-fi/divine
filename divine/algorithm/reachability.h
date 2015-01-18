// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <brick-bitlevel.h>

#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h>

#include <divine/graph/ltlce.h>

#ifndef DIVINE_ALGORITHM_REACHABILITY_H
#define DIVINE_ALGORITHM_REACHABILITY_H

namespace divine {
namespace algorithm {

template < typename Vertex >
struct ReachabilityShared {
    using Handle = typename Vertex::Handle;

    Handle goal;
    typename Vertex::Node goalData;
    bool deadlocked;
    algorithm::Statistics stats;
    CeShared< Blob, Handle > ce;
    bool need_expand;
    ReachabilityShared() : need_expand( false ) {}
};

template< typename BS, typename Vertex >
typename BS::bitstream &operator<<( BS &bs, ReachabilityShared< Vertex > st )
{
    return bs << st.goal << st.goalData << st.deadlocked << st.stats << st.ce << st.need_expand;
}

template< typename BS, typename Vertex >
typename BS::bitstream &operator>>( BS &bs, ReachabilityShared< Vertex > &st )
{
    return bs >> st.goal >> st.goalData >> st.deadlocked >> st.stats >> st.ce >> st.need_expand;
}

template< typename T >
struct Identity { using Type = T; };

template< typename T >
struct Const { template< typename > struct Wrap { using Type = T; }; };

using brick::bitlevel::compiletime::sizeOf;

/**
 * A simple parallel reachability analysis implementation. Nothing to worry
 * about here.
 */
template< template< typename > class E, typename Setup,
    template< typename > class Sh = ReachabilityShared,
    template< typename > class Wrap = Identity >
struct CommonReachability : Algorithm, AlgorithmUtils< Setup, Sh< typename Setup::Store::Vertex > >,
                            Wrap< Parallel< Setup::template Topology, CommonReachability< E, Setup, Sh, Wrap > > >::Type
{
    using This = CommonReachability< E, Setup, Sh, Wrap >;
    using Shared = Sh< typename Setup::Store::Vertex >;
    using Utils = AlgorithmUtils< Setup, Shared >;

    ALGORITHM_CLASS( Setup );

    BRICK_RPC( Utils, &This::_visit, &This::_por, &This::_por_worker,
                      &This::_parentTrace, &This::_successorTrace, &This::_ceIsInitial );

    using Extension = E< Store >;

    using Utils::shared;
    using Utils::shareds;

    Handle goal;
    Node goalData;
    bool deadlocked;

    typedef LtlCE< Setup, Shared, Extension, typename Store::Hasher > CE;
    CE ce;

    Pool& pool() {
        return this->graph().pool();
    }

    Extension &extension( Vertex v ) {
        return v.template extension< Extension >();
    }

    struct Main : Visit< This, Setup >
    {
        static visitor::ExpansionAction expansion( This &r, Vertex st )
        {
            r.shared.stats.addNode( r.graph(), st );
            return visitor::ExpansionAction::Expand;
        }

        static void setGoal( This &r, Vertex v, bool deadlock ) {
            r.shared.goal = v.handle();
            r.shared.goalData = r.pool().allocate( r.pool().size( v.node() ) );
            r.pool().copy( v.node(), r.shared.goalData );
            ASSERT( r.store().valid( r.shared.goal ) );
            r.shared.deadlocked = deadlock;
        }

        static visitor::TransitionAction transition( This &r, Vertex f, Vertex t, Label )
        {
            if ( !r.store().valid( r.extension( t ).parent() ) )
                r.extension( t ).parent() = f.handle();

            r.shared.stats.addEdge( r.store(), f, t );

            if ( r.meta().input.propertyType == graph::PT_Goal
                 && r.graph().stateFlags( t.node(), graph::flags::isGoal ) )
            {
                setGoal( r, t, false );
                return visitor::TransitionAction::Terminate;
            }

            r.graph().porTransition( r.store(), f, t );
            return visitor::TransitionAction::Follow;
        }

        static visitor::DeadlockAction deadlocked( This &r, Vertex n )
        {
            r.shared.stats.addDeadlock();

            if ( r.meta().input.propertyType != graph::PT_Deadlock )
                return visitor::DeadlockAction::Ignore;

            setGoal( r, n, true );
            return visitor::DeadlockAction::Terminate;
        }
    };

    void _visit() { // parallel
        this->visit( this, Main(), shared.need_expand );
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

    CommonReachability( Meta m ) : Algorithm( m, sizeOf< Extension >() )
    {
        this->init( *this );
    }

    CommonReachability( This &master, std::pair< int, int > id )
        : Algorithm( master.meta(), sizeOf< Extension >() )
    {
        this->init( *this, master, id );
    }

    Shared runCe( Shared sh, void (CE::*ceCall)( This&, typename Setup::Store& ) ) {
        shared = sh;
        ce.setup( *this, shared );
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

    virtual void counterexample( Handle n ) {
        shared.ce.initial = n;
        ce.setup( *this, shared );
        Node goal = ce.linear( *this, *this );
        ce.goal( *this, goal );
    }

    void collect() {
        deadlocked = false;
        for ( int i = 0; i < int( shareds.size() ); ++i ) {
            shareds[ i ].stats.update( meta().statistics );
            if ( this->store().valid( shareds[ i ].goal ) ) {
                goal = shareds[ i ].goal;
                goalData = shareds[ i ].goalData;
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

        if ( this->store().valid( goal ) && this->meta().output.wantCe ) {
            counterexample( goal );
            result().ceType = deadlocked ? meta::Result::CET::Deadlock : meta::Result::CET::Goal;
        }

        result().propertyHolds = this->store().valid( goal )
                                  ? meta::Result::R::No : meta::Result::R::Yes;
        result().fullyExplored = this->store().valid( goal )
                                  ? meta::Result::R::No : meta::Result::R::Yes;

        resultBanner( !this->store().valid( goal ) );
    }
};

template< typename Store >
struct Extension {
    using Handle = typename Store::Handle;

    Handle _parent;
    Handle &parent() {
        return _parent;
    }
};

template< typename Store >
struct NoExtension {
    using Handle = typename Store::Handle;
    Handle parent() { return Handle(); }
};

template< typename Setup >
using Reachability = CommonReachability< Extension, Setup >;

template< typename Setup >
struct WeakReachability : CommonReachability< NoExtension, Setup > {

    using Parent = CommonReachability< NoExtension, Setup >;
    using Handle = typename Parent::Handle;

    template< typename... Args >
    WeakReachability( Args &&... args ) :
        Parent( std::forward< Args >( args )... )
    {}

    virtual void counterexample( Handle ) {
        this->ce.setup( *this, this->shared );
        this->ce.goal( *this, this->goalData, false );
    }

};

}
}

#endif
