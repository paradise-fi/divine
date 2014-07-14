// -*- C++ -*- (c) 2014 Vladimír Štill <xstill@fi.muni.cz>

#include <type_traits>

#include <divine/graph/label.h>
#include <divine/algorithm/reachability.h>

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
struct CsdrShared : ReachabilityShared< Vertex > {
    int32_t level;
    ReachabilityShared< Vertex > &base() { return *this; }
};

template< typename BS, typename Vertex >
typename BS::bitstream &operator<<( BS &bs, CsdrShared< Vertex > st )
{
    return bs << st.base() << st.level;
}

template< typename BS, typename Vertex >
typename BS::bitstream &operator>>( BS &bs, CsdrShared< Vertex > &st )
{
    return bs >> st.base() >> st.level;
}

template< typename Store >
struct CsdrExtension {
    using Handle = typename Store::Handle;

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

    CsdrExtension &registerPredecessor( const CsdrExtension &from, graph::ControlLabel label ) {
        int32_t level = from.level() + 1;
        if ( label != graph::ControlLabel::NoSwitch )
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
    CsdrExtension &registerPredecessor( const CsdrExtension &, Label ) = delete;

  private:
    Handle _parent;
    typename Store::template DataWrapper< int32_t > _level;
};

/**
 * A simple parallel reachability analysis implementation. Nothing to worry
 * about here.
 */
template< typename Setup >
struct Csdr : CommonReachability< CsdrExtension, Setup, CsdrShared,
    Const< Parallel< Setup::template Topology, Csdr< Setup > > >::template Wrap >
{
    using This = Csdr< Setup >;
    using Shared = CsdrShared< typename Setup::Store::Vertex >;
    using Base = CommonReachability< CsdrExtension, Setup, CsdrShared,
          Const< Parallel< Setup::template Topology, Csdr< Setup > > >::template Wrap >;

    ALGORITHM_CLASS( Setup );

    DIVINE_RPC( Base, &This::_visit );

    using Extension = typename Base::Extension;
    using Base::shared;
    using Base::shareds;
    using Base::pool;
    using Base::meta;
    using Base::progress;
    using Base::result;
    using Base::parallel;
    using Base::ring;
    using Base::counterexample;
    using Base::resultBanner;

    Handle goal;
    Node goalData;
    bool deadlocked;
    long thisLevel;
    int32_t limit;

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

    Csdr( Meta m ) : Base( m ), limit( std::numeric_limits< int32_t >::max() )
    {
        if ( meta().algorithm.contextSwitchLimit > 0 )
            limit = meta().algorithm.contextSwitchLimit;
    }

    Csdr( This &master, std::pair< int, int > id ) :
        Base( master, id ), limit( master.limit )
    { }

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
