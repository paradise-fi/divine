// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h>
#include <divine/ltlce.h>

#ifndef DIVINE_ALGORITHM_MAP_H
#define DIVINE_ALGORITHM_MAP_H

namespace divine {
namespace algorithm {

struct VertexId {
    uintptr_t ptr;
    short owner;

    bool operator<( const VertexId other ) const {
        if ( owner < other.owner )
            return true;
        if ( owner > other.owner )
            return false;
        return ptr < other.ptr;
    }

    bool operator!=( const VertexId other ) const {
        return ptr != other.ptr || owner != other.owner;
    }

    bool operator==( const VertexId other ) const {
            return ptr == other.ptr && owner == other.owner;
    }

    bool valid() const {
        return ptr != 0;
    }

    friend std::ostream &operator<<( std::ostream &o, VertexId i ) {
        return o << "(" << i.owner << ", " << i.ptr << "[" << i.ptr % 1024 << "])";
    }
} __attribute__((packed));


struct MapShared {
    int expanded, eliminated, accepting;
    int iteration;
    CeShared< Blob > ce;
    algorithm::Statistics stats;
    bool need_expand;
};

template< typename BS >
typename BS::bitstream &operator<<( BS &bs, const MapShared &sh )
{
    return bs << sh.stats << sh.ce << sh.iteration << sh.need_expand
              << sh.accepting << sh.expanded << sh.eliminated;
}

template< typename BS >
typename BS::bitstream &operator>>( BS &bs, MapShared &sh )
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
template< typename G, template< typename > class Topology, typename _Statistics >
struct Map : Algorithm, AlgorithmUtils< G >, Parallel< Topology, Map< G, Topology, _Statistics > >
{
    typedef Map< G, Topology, _Statistics > This;
    ALGORITHM_CLASS( G, MapShared );

    int d_eliminated,
        acceptingCount,
        eliminated,
        expanded;
    Node cycle_node;

    VertexId makeId( Node n ) {
        VertexId ret;
        ret.ptr = reinterpret_cast< intptr_t >( n.data() );
        ret.owner = this->id();
        return ret;
    }

    struct Extension {
        Blob parent;
        bool seen:1;
        short iteration:14;
        // elim: 0 = candidate for elimination, 1 = not a canditate, 2 = eliminated
        unsigned short elim:2;
        VertexId map;
        VertexId oldmap;
    };

    LtlCE< G, Shared, Extension > ce;

    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    visitor::TransitionAction updateIteration( Node t ) {
        int old = extension( t ).iteration;
        extension( t ).iteration = shared.iteration;
        return (old != shared.iteration) ?
            visitor::ExpandTransition :
            visitor::ForgetTransition;
    }

    void collect() {
        for ( int i = 0; i < shareds.size(); ++i )
            shared.stats.merge( shareds[ i ].stats );
        shared.stats.update( meta().statistics );
        shared.stats = algorithm::Statistics();
        for ( int i = 0; i < shareds.size(); ++ i ) {
            if ( shared.iteration == 1 )
                acceptingCount += shareds[ i ].accepting;
            d_eliminated += shareds[ i ].eliminated;
            expanded += shareds[ i ].expanded;
            assert_eq( shared.eliminated, 0 );
            assert_eq( shared.expanded, 0 );

            if ( shareds[ i ].ce.initial.valid() )
                cycle_node = shareds[ i ].ce.initial;
        }
    }

    bool isAccepting( Node st ) {
        if ( !m_graph.isAccepting ( st ) )
            return false;
        if ( extension( st ).elim == 2 )
            return false;
        return true;
    }

    visitor::ExpansionAction expansion( Node st )
    {
        ++ shared.expanded;
        if ( !extension( st ).seen ) {
            extension( st ).seen = true;
            if ( m_graph.isAccepting ( st ) )
                ++ shared.accepting;
            m_graph.porExpansion( st );
            shared.stats.addNode( m_graph, st );
        } else
            shared.stats.addExpansion();

        return visitor::ExpandState;
    }

    visitor::TransitionAction transition( Node f, Node t )
    {
        if ( shared.iteration == 1 )
            m_graph.porTransition( f, t, 0 );

        if ( !f.valid() ) {
            assert( equal( t, m_graph.initial() ) );
            return updateIteration( t );
        }

        if ( !extension( t ).parent.valid() )
            extension( t ).parent = f;

        /* self loop */
        if ( m_graph.isAccepting( f ) && f.pointer() == t.pointer() ) {
            shared.ce.initial = t;
            return visitor::TerminateOnTransition;
        }

        /* MAP arrived to its origin */
        if ( isAccepting( t ) && extension( f ).map == makeId( t ) ) {
            shared.ce.initial = t;
            return visitor::TerminateOnTransition;
        }

        if ( extension( f ).oldmap.valid() && extension( t ).oldmap.valid() )
            if ( extension( f ).oldmap != extension( t ).oldmap )
                return updateIteration( t );

        VertexId map = std::max( extension( f ).map, extension( t ).map );
        if ( isAccepting( t ) )
            map = std::max( map, makeId( t ) );

        if ( extension( t ).map < map ) {
            // we are *not* the MAP of our successors anymore, so not a
            // candidate for elimination (shrinking), remove from set
            if ( isAccepting( t ) && extension( t ).elim )
                extension( t ).elim = 1;
            extension( t ).map = map;
            return visitor::ExpandTransition;
        }

        return updateIteration( t );
    }

    void _visit() {
        typedef visitor::Setup< G, This, typename This::Table, _Statistics > Setup;
        typedef visitor::Partitioned< Setup, This, Hasher > Visitor;

        shared.expanded = 0;
        shared.eliminated = 0;
        this->comms().notify( this->id(), &m_graph.pool() );

        Visitor visitor( m_graph, *this, *this, hasher, &this->table() );
        m_graph.queueInitials( visitor );
        visitor.processQueue();
    }

    void _cleanup() {
        for ( size_t i = 0; i < this->table().size(); ++i ) {
            Node st = this->table()[ i ];
            if ( st.valid() ) {
                extension( st ).oldmap = extension( st ).map;
                extension( st ).map = VertexId();
                if ( isAccepting( st ) ) {
                    /* elim == 1 means NOT to be eliminated (!) */
                    if ( extension( st ).elim == 1 )
                        extension( st ).elim = 0;
                    else {
                        extension( st ).elim = 2;
                        ++ shared.eliminated;
                    }
                }
            }
        }
        while ( !this->idle() ) ;
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

    void visit() {
        parallel( &This::_visit );
        collect();

        while ( !cycle_node.valid() && shared.iteration == 1 ) {
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

    Shared _parentTrace( Shared sh ) {
        shared = sh;
        ce.setup( m_graph, shared );
        ce._parentTrace( *this, hasher, equal, this->table() );
        return shared;
    }

    void _traceCycle() {
        ce._traceCycle( *this, hasher, this->table() );
    }

    void run()
    {
        shared.iteration = 1;
        acceptingCount = eliminated = d_eliminated = expanded = 0;
        result().fullyExplored = meta::Result::No;
        bool valid = true;
        do {
            progress() << " iteration " << std::setw( 3 ) << shared.iteration
                       << "...\t" << std::flush;
            shared.accepting = shared.eliminated = shared.expanded = 0;
            expanded = d_eliminated = 0;
            visit();
            eliminated += d_eliminated;
            assert_leq( eliminated, acceptingCount );
            progress() << eliminated << " eliminated, "
                       << expanded << " expanded" << std::endl;
            valid = !cycle_node.valid();

            if ( valid )
                result().fullyExplored = meta::Result::Yes;

            ++ shared.iteration;
        } while ( d_eliminated > 0 && eliminated < acceptingCount && valid );

        result().propertyHolds = valid ? meta::Result::Yes : meta::Result::No;
        meta().statistics.deadlocks = -1; /* did not count */
        meta().statistics.transitions = -1; /* cannot count */

        livenessBanner( valid );

        if ( !valid && want_ce ) {
            progress() << " generating counterexample...     " << std::flush;
            assert( cycle_node.valid() );
            shared.ce.initial = cycle_node;
            ce.setup( m_graph, shared );
            ce.lasso( *this, *this );
            progress() << "done" << std::endl;
            result().ceType = meta::Result::Cycle;
        }
    }

    Map( Meta m, bool master = false )
        : Algorithm( m, sizeof( Extension ) )
    {
        if ( master )
            this->becomeMaster( m.execution.threads, m );
        this->init( this );
    }

};

ALGORITHM_RPC( Map );
ALGORITHM_RPC_ID( Map, 1, _visit );
ALGORITHM_RPC_ID( Map, 2, _cleanup );
ALGORITHM_RPC_ID( Map, 3, _parentTrace );
ALGORITHM_RPC_ID( Map, 4, _traceCycle );
ALGORITHM_RPC_ID( Map, 5, _por );
ALGORITHM_RPC_ID( Map, 6, _por_worker );

}
}

#endif
