// -*- C++ -*- (c) 2014 Vladimír Štill <xstill@fi.muni.cz>

#include <type_traits>

#include <divine/toolkit/label.h>
#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h>
#include <divine/graph/ltlce.h>

#ifndef DIVINE_ALGORITHM_CSDR_H
#define DIVINE_ALGORITHM_CSDR_H

namespace divine {
namespace algorithm {

template< typename T, typename = decltype( T() < T() ) >
void setToMin( T &target, const T &val ) {
    if ( val < target )
        target = val;
}

template< typename T, typename = decltype( T() < T() ) >
void setToMin( std::atomic< T > &target, const T &val ) {
    T tmp = target;
    while ( val < tmp )
        target.compare_exchange_weak( tmp, val );
}

template < typename Vertex >
struct CsdrShared {
    using Handle = typename Vertex::Handle;
    using Node = typename Vertex::Node;

    algorithm::Statistics stats;
    Handle goal;
    Node goalData;
    CeShared< Blob, Handle > ce;
    int32_t level;
    bool deadlocked;
    bool need_expand;
    CsdrShared() : need_expand( false ) {}
};

template< typename BS, typename Vertex >
typename BS::bitstream &operator<<( BS &bs, CsdrShared< Vertex > st )
{
    return bs << st.goal << st.goalData << st.deadlocked << st.stats << st.ce << st.need_expand << st.level;
}

template< typename BS, typename Vertex >
typename BS::bitstream &operator>>( BS &bs, CsdrShared< Vertex > &st )
{
    return bs >> st.goal >> st.goalData >> st.deadlocked >> st.stats >> st.ce >> st.need_expand >> st.level;
}

/**
 * A simple parallel reachability analysis implementation. Nothing to worry
 * about here.
 */
template< typename Setup >
struct Csdr : Algorithm, AlgorithmUtils< Setup, CsdrShared< typename Setup::Store::Vertex > >,
                            Parallel< Setup::template Topology, Csdr< Setup > >
{
    using This = Csdr< Setup >;
    using Shared = CsdrShared< typename Setup::Store::Vertex >;
    using Utils = AlgorithmUtils< Setup, Shared >;

    ALGORITHM_CLASS( Setup );

    DIVINE_RPC( Utils, &This::_visit, &This::_por, &This::_por_worker,
                       &This::_parentTrace, &This::_successorTrace, &This::_ceIsInitial );

    struct Extension {
        Handle &parent() { return _parent; }
        int32_t level() const {
            int32_t l = std::abs( _level );
            assert_leq( 1, l );
            return l - 1;
        }

        bool initialized() const { return _level; }
        bool done() const { return _level < 0; }

        bool setDone() {
            if ( done() )
                return true;
            assert_leq( 1, int32_t( _level ) );
            _level = -_level;
            return false;
        }

        void setInit() {
            if ( !_level )
                _level = 1;
        }

        Extension &registerPredecessor( const Extension &from, toolkit::ControlLabel label ) {
            int32_t level = from.level() + 1;
            if ( label != toolkit::ControlLabel::NoSwitch )
                ++level;
            assert_leq( 1, level ); // check for overflow
            if ( initialized() )
                setToMin( _level, level );
            else
                _level = level;
            return *this;
        }

        // we cannot support any other type of labels, please do not
        // instentiate with them
        template< typename Label >
        Extension &registerPredecessor( const Extension &, Label ) = delete;

      private:
        Handle _parent;
        typename Store::template DataWrapper< int32_t > _level;
    };

    using Utils::shared;
    using Utils::shareds;

    Handle goal;
    Node goalData;
    bool deadlocked;
    long thisLevel;
    int32_t limit;

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
        using Base = Visit< This, Setup >;

        static visitor::ExpansionAction expansion( This &c, Vertex st )
        {
            c.shared.stats.addNode( c.graph(), st );
            return c.extension( st ).setDone()
                ? visitor::ExpansionAction::Ignore
                : visitor::ExpansionAction::Expand;
        }

        static void setGoal( This &c, Vertex v, bool deadlock ) {
            c.shared.goal = v.handle();
            c.shared.goalData = c.pool().allocate( c.pool().size( v.node() ) );
            c.pool().copy( v.node(), c.shared.goalData );
            assert( c.store().valid( c.shared.goal ) );
            c.shared.deadlocked = deadlock;
        }

        static visitor::TransitionAction transition( This &c, Vertex f, Vertex t, Label l )
        {
            c.shared.stats.addEdge( c.store(), f, t );

            if ( !c.store().valid( f ) )
                c.extension( t ).setInit();
            else {
                if ( !c.store().valid( c.extension( t ).parent() ) )
                    c.extension( t ).parent() = f.handle();
                if ( c.extension( t ).done()
                    || c.extension( t ).registerPredecessor( c.extension( f ), l ).level() > c.shared.level )
                    return visitor::TransitionAction::Forget;
            }

            // on our level
            if ( c.meta().input.propertyType == graph::PT_Goal
                 && c.graph().isGoal( t.node() ) )
            {
                setGoal( c, t, false );
                return visitor::TransitionAction::Terminate;
            }

            c.graph().porTransition( c.store(), f, t );
            return visitor::TransitionAction::Expand;
        }

        static visitor::DeadlockAction deadlocked( This &c, Vertex n )
        {
            c.shared.stats.addDeadlock();

            if ( c.meta().input.propertyType != graph::PT_Deadlock )
                return visitor::DeadlockAction::Ignore;

            setGoal( c, n, true );
            return visitor::DeadlockAction::Terminate;
        }

        template< typename Visitor >
        void queueInitials( This &t, Visitor &v ) {
            if ( t.shared.level == 0 )
                Base::queueInitials( t, v );
            else
                for ( auto st : t.store() )
                    if ( t.extension( st ).level() == t.shared.level ) {
                        v.queue( Vertex(), st.node(), Label() );
                        st.disown();
                    }
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

    Csdr( Meta m ) :
        Algorithm( m, bitops::compiletime::sizeOf< Extension >() ),
        limit( std::numeric_limits< int32_t >::max() )
    {
        this->init( *this );
        if ( meta().algorithm.contextSwitchLimit > 0 )
            limit = meta().algorithm.contextSwitchLimit;
    }

    Csdr( This &master, std::pair< int, int > id ) :
        Algorithm( master.meta(), bitops::compiletime::sizeOf< Extension >() ),
        limit( master.limit )
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
        long old = meta().statistics.visited;
        for ( int i = 0; i < int( shareds.size() ); ++i ) {
            shareds[ i ].stats.update( meta().statistics );
            if ( this->store().valid( shareds[ i ].goal ) ) {
                goal = shareds[ i ].goal;
                goalData = shareds[ i ].goalData;
                if ( shareds[ i ].deadlocked )
                    deadlocked = true;
            }
        }
        thisLevel = meta().statistics.visited - old;
    }


    void run() {
        for ( int32_t level = 0; level <= limit; ++level ) {
            progress() << "  level " << level << "... \t " << std::flush;

            shared.level = level;

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
                       << meta().statistics.transitions << " edges, "
                       << thisLevel << " states in this level" << std::flush;

            if ( this->store().valid( goal ) ) {
                if ( deadlocked )
                    progress() << ", DEADLOCK";
                else
                    progress() << ", GOAL";
                break;
            }
            if ( thisLevel == 0 ) {
                progress() << ", DONE";
                break;
            }
            progress() << std::endl;
        }

        progress() << std::endl;

        if ( this->store().valid( goal ) && this->meta().output.wantCe ) {
            counterexample( goal );
            result().ceType = deadlocked ? meta::Result::CET::Deadlock : meta::Result::CET::Goal;
        }

        result().propertyHolds = this->store().valid( goal )
                                  ? meta::Result::R::No : meta::Result::R::Yes;
        result().fullyExplored = this->store().valid( goal ) || thisLevel > 0
                                  ? meta::Result::R::No : meta::Result::R::Yes;

        resultBanner( !this->store().valid( goal ) );
    }
};

} // namespace algorithm
} // namespace divine

#endif // DIVINE_ALGORITHM_CSDR_H
