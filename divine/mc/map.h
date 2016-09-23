// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/alg/common.h>
#include <divine/alg/metrics.h>
#include <divine/graph/ltlce.h>

#ifndef DIVINE_ALGORITHM_MAP_H
#define DIVINE_ALGORITHM_MAP_H

namespace divine {
namespace algorithm {

struct MapVertexId {
    uint64_t raw;

    bool valid() const {
        return raw != Blob().raw();
    }

    friend std::ostream &operator<<( std::ostream &o, MapVertexId i ) {
        return o << i.raw;
    }

    MapVertexId() = default;
    template< typename Handle >
    MapVertexId( Handle h ) : raw( h.asNumber() ) { }
} __attribute__((packed));

static inline bool operator==( MapVertexId a, MapVertexId b ) {
    return a.raw == b.raw;
}

static inline bool operator!=( MapVertexId a, MapVertexId b ) {
    return a.raw != b.raw;
}

static inline bool operator<( MapVertexId a, MapVertexId b ) {
    return a.raw < b.raw;
}

template < typename Handle >
struct MapShared {
    int64_t expanded, eliminated, accepting;
    int iteration;
    CeShared< Blob, Handle > ce;
    algorithm::Statistics stats;
    bool need_expand;
};

template< typename BS, typename Handle >
typename BS::bitstream &operator<<( BS &bs, const MapShared< Handle > &sh )
{
    return bs << sh.stats << sh.ce << sh.iteration << sh.need_expand
              << sh.accepting << sh.expanded << sh.eliminated;
}

template< typename BS, typename Handle >
typename BS::bitstream &operator>>( BS &bs, MapShared< Handle > &sh )
{
    return bs >> sh.stats >> sh.ce >> sh.iteration >> sh.need_expand
              >> sh.accepting >> sh.expanded >> sh.eliminated;
}

/**
 * Implementation of Maximal Accepting Predecessors fair cycle detection
 * algorithm. L. Brim, I. Černá, P. Moravec, and J. Šimša. Accepting
 * Predecessors are a Better than Back Edges in Distributed LTL
 * Model-Checking. In 5th International Conference on Formal Methods in
 * Computer-Aided Design (FM-CAD'04), volume 3312 of LNCS, pages
 * 352–366. Springer-Verlag, 2004.
 */
template< typename Setup >
struct Map : Algorithm, AlgorithmUtils< Setup, MapShared< typename Setup::Store::Handle > >,
             Parallel< Setup::template Topology, Map< Setup > >
{
    using This = Map< Setup >;
    using Shared = MapShared< typename Setup::Store::Handle >;
    using Utils = AlgorithmUtils< Setup, Shared >;

    using Utils::shared;
    using Utils::shareds;

    ALGORITHM_CLASS( Setup );
    BRICK_RPC( Utils, &This::_visit, &This::_cleanup,
                      &This::_parentTrace, &This::_traceCycle,
                      &This::_por, &This::_por_worker,
                      &This::_successorTrace, &This::_ceIsInitial );

    int64_t acceptingCount,
            eliminated,
            expanded;
    Handle cycle_node;

    MapVertexId makeId( Vertex n ) {
        return MapVertexId( n.handle() );
    }

    struct Extension {
        BitTuple<
            BitField< Handle >,
            BitField< MapVertexId >,
            BitField< MapVertexId >,
            BitLock,
            BitField< bool, 1 >,// seen
            BitField< short, 13 >,// iteration
            BitField< unsigned short, 2 >// elim
        > data;

        void lock() { get< 3 >( data ).lock(); }
        void unlock() { get< 3 >( data ).unlock(); }

        auto parent() -> decltype( get< 0 >( data ) ) {
            return get< 0 >( data );
        }
        auto map() -> decltype( get< 1 >( data ) ) {
            return get< 1 >( data );
        }
        auto oldmap() -> decltype( get< 2 >( data ) ) {
            return get< 2 >( data );
        }
        auto seen() -> decltype( get< 4 >( data ) ) {
            return get< 4 >( data );
        }
        auto iteration() -> decltype( get< 5 >( data ) ) {
            return get< 5 >( data );
        }
        auto elim() -> decltype( get< 6 >( data ) ) {
            return get< 6 >( data );
        }
    };

    using Guard = typename Store::template Guard< Extension >;

    typedef LtlCE< Setup, Shared, Extension, typename Store::Hasher > CE;
    CE ce;

    Extension &extension( Vertex v ) {
        return v.template extension< Extension >();
    }

    visitor::TransitionAction updateIteration( Vertex t ) {
        int old = extension( t ).iteration();
        extension( t ).iteration() = shared.iteration;
        return (old != shared.iteration) ?
            visitor::TransitionAction::Expand :
            visitor::TransitionAction::Forget;
    }

    void collect() {
        eliminated = 0;
        for ( int i = 0; i < int( shareds.size() ); ++i )
            shared.stats.merge( shareds[ i ].stats );
        shared.stats.update( meta().statistics );
        shared.stats = algorithm::Statistics();
        for ( int i = 0; i < int( shareds.size() ); ++ i ) {
            if ( shared.iteration == 1 )
                acceptingCount += shareds[ i ].accepting;
            eliminated += shareds[ i ].eliminated;
            expanded += shareds[ i ].expanded;
            ASSERT_EQ( shared.eliminated, 0 );
            ASSERT_EQ( shared.expanded, 0 );

            if ( this->store().valid( shareds[ i ].ce.initial ) )
                cycle_node = shareds[ i ].ce.initial;
        }
    }

    bool isAccepting( Vertex st ) {
        if ( extension( st ).elim() >= 2 )
            return false;
        if ( !this->graph().stateFlags( st.node(), graph::flags::isAccepting ) ) {
            extension( st ).elim() = 3;
            return false;
        }
        return true;
    }

    struct Main : Visit< This, Setup >
    {
        static visitor::ExpansionAction expansion( This &m, Vertex st )
        {
            Guard guard( st );
            ++ m.shared.expanded;
            if ( !m.extension( st ).seen() ) {
                m.extension( st ).seen() = true;
                if ( m.graph().stateFlags( st.node(), graph::flags::isAccepting ) )
                    ++ m.shared.accepting;
                else
                    m.extension( st ).elim() = 3; // not accepting
                m.shared.stats.addNode( m.graph(), st );
            } else
                m.shared.stats.addExpansion();

            return visitor::ExpansionAction::Expand;
        }

        static visitor::TransitionAction transition( This &m, Vertex f, Vertex t, Label )
        {
            Guard guard( f, t );
            if ( m.shared.iteration == 1 )
                m.graph().porTransition( m.store(), f, t );

            if ( !m.store().valid( f ) )
                return m.updateIteration( t );

            if ( !m.store().valid( m.extension( t ).parent() ) )
                m.extension( t ).parent() = f.handle();

            /* self loop */
            if ( m.graph().stateFlags( f.node(), graph::flags::isAccepting ) &&
                 m.store().equal( f.handle(), t.handle() ) )
            {
                m.shared.ce.initial = t.handle();
                return visitor::TransitionAction::Terminate;
            }

            /* MAP arrived to its origin */
            if ( m.isAccepting( t ) && m.extension( f ).map() == m.makeId( t ) ) {
                m.shared.ce.initial = t.handle();
                return visitor::TransitionAction::Terminate;
            }

            if ( m.extension( f ).oldmap().get().valid() && m.extension( t ).oldmap().get().valid() )
                if ( m.extension( f ).oldmap() != m.extension( t ).oldmap() )
                    return m.updateIteration( t );

            MapVertexId map = std::max( m.extension( f ).map().get(), m.extension( t ).map().get() );
            if ( m.isAccepting( t ) )
                map = std::max( map, m.makeId( t ) );

            if ( m.extension( t ).map() < map ) {
                // we are *not* the MAP of our successors anymore, so not a
                // candidate for elimination (shrinking), remove from set
                // elim == 0 means we are candidate for elimination now
                if ( m.isAccepting( t ) && m.extension( t ).elim() == 0 && m.makeId( t ) < map )
                    m.extension( t ).elim() = 1;
                m.extension( t ).map() = map;
                return visitor::TransitionAction::Expand;
            }

            return m.updateIteration( t );
        }
    };

    void _visit() {
        shared.expanded = 0;
        shared.eliminated = 0;
        this->visit( this, Main() );
    }

    void _cleanup() {
        for ( auto st : this->store() ) {
            if ( this->store().valid( st ) ) {
                extension( st ).oldmap() = extension( st ).map();
                extension( st ).map() = MapVertexId();
//                 if ( isAccepting( st ) ) {
                if ( extension( st ).elim() != 3 ) {
                    /* elim == 1 means NOT to be eliminated (!) */
                    if ( extension( st ).elim() == 1 )
                        extension( st ).elim() = 0;
                    else {
                        extension( st ).elim() = 2;
                        ++ shared.eliminated;
                    }
                }
            }
        }
        while ( !this->idle() ) ;
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

    void iteration() {
        parallel( &This::_visit );
        collect();

        while ( !this->store().valid( cycle_node ) && shared.iteration == 1 ) {
            shared.need_expand = false;
            ring( &This::_por );

            if ( shared.need_expand ) {
                parallel( &This::_visit );
                collect();
            } else
                break;
        }

        parallel( &This::_cleanup );
        collect();
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

    void _traceCycle() {
        ce._traceCycle( *this );
    }

    void run()
    {
        int64_t old_eliminated = 0;
        shared.iteration = 1;
        acceptingCount = eliminated = expanded = 0;
        result().fullyExplored = meta::Result::R::No;
        bool valid = true;
        do {
            progress() << " iteration " << std::setw( 3 ) << shared.iteration
                       << "...\t" << std::flush;
            shared.accepting = shared.eliminated = shared.expanded = 0;
            old_eliminated = eliminated;
            expanded = 0;
            iteration();
            ASSERT_LEQ( eliminated, acceptingCount );
            progress() << eliminated << " eliminated, "
                       << expanded << " expanded" << std::endl;
            valid = !this->store().valid( cycle_node );

            if ( valid )
                result().fullyExplored = meta::Result::R::Yes;

            ++ shared.iteration;
        } while ( ( eliminated - old_eliminated ) > 0
                && eliminated < acceptingCount && valid );

        result().propertyHolds = valid ? meta::Result::R::Yes : meta::Result::R::No;
        meta().statistics.deadlocks = -1; /* did not count */
        meta().statistics.transitions = -1; /* cannot count */

        resultBanner( valid );

        if ( !valid && want_ce ) {
            progress() << " generating counterexample...     " << std::flush;
            ASSERT( this->store().valid( cycle_node ) );
            shared.ce.initial = cycle_node;
            ce.setup( *this, shared );
            ce.lasso( *this, *this );
            progress() << "done" << std::endl;
            result().ceType = meta::Result::CET::Cycle;
        }
    }

    Map( Meta m, typename Setup::Generator *g = nullptr )
        : Algorithm( m, sizeof( Extension ) )
    {
        this->init( *this, g );
    }

    Map( Map &master, std::pair< int, int > id ) : Algorithm( master.meta(), sizeof( Extension ) )
    {
        this->init( *this, master, id );
    }

};

}
}

#endif
