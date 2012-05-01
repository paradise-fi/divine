// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/algorithm/map.h> // for VertexId
#include <divine/ltlce.h>
#include <divine/visitor.h>
#include <divine/parallel.h>

#ifndef DIVINE_ALGORITHM_OWCTY_H
#define DIVINE_ALGORITHM_OWCTY_H

namespace divine {
namespace algorithm {

struct OwctyShared {
    size_t size, oldsize;
    Blob cycle_node;
    bool cycle_found;
    int iteration;
    CeShared< Blob > ce;
    algorithm::Statistics stats;
    bool need_expand;

    OwctyShared() : cycle_found( false ) {}
};

static inline rpc::bitstream &operator<<( rpc::bitstream &bs, const OwctyShared &sh )
{
    return bs << sh.size << sh.oldsize << sh.cycle_node << sh.cycle_found
              << sh.iteration << sh.ce << sh.stats << sh.need_expand;
}

static inline rpc::bitstream &operator>>( rpc::bitstream &bs, OwctyShared &sh )
{
    return bs >> sh.size >> sh.oldsize >> sh.cycle_node >> sh.cycle_found
              >> sh.iteration >> sh.ce >> sh.stats >> sh.need_expand;
}

/**
 * Implementation of the One-Way Catch Them Young algorithm for fair cycle
 * detection, as described in I. Černá and R. Pelánek. Distributed Explicit
 * Fair Cycle Detection (Set a a Based Approach). In T. Ball and S.K. Rajamani,
 * editors, Model Checking Software, the 10th International SPIN Workshop,
 * volume 2648 of LNCS, pages 49 – 73. Springer-Verlag, 2003.
 *
 * Extended to work on-the-fly, as described in J. Barnat, L. Brim, and
 * P. Ročkai. An Optimal On-the-fly Parallel Algorithm for Model Checking of
 * Weak LTL Properties. In International Conference on Formal Engineering
 * Methods, LNCS. Springer-Verlag, 2009.  To appear.
 */
template< typename G, template< typename > class Topology, typename Statistics >
struct Owcty : Algorithm, AlgorithmUtils< G >, Parallel< Topology, Owcty< G, Topology, Statistics > >
{
    typedef Owcty< G, Topology, Statistics > This;
    ALGORITHM_CLASS( G, OwctyShared );

    // -------------------------------
    // -- Some useful types
    // --

    struct Extension {
        Blob parent;
        uintptr_t map:(8 * sizeof(uintptr_t) - 2);
        bool inS:1;
        bool inF:1;

        // ... the last word is a bit crammed, so consider just extending this
        size_t predCount:14; // up to 16k predecessors per node
        size_t iteration:9; // handle up to 512 iterations
        unsigned map_owner:9; // handle up to 512 MPI nodes
    };

    LtlCE< G, Shared, Extension > ce;

    // ------------------------------------------------------
    // -- generally useful utilities
    // --

    static Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    static void updatePredCount( Node n, int p ) {
        extension( n ).predCount = p;
    }

    bool cycleFound() {
        if ( shared.cycle_found )
            return true;
        for ( int i = 0; i < shareds.size(); ++i ) {
            if ( shareds[ i ].cycle_found )
                return true;
        }
        return false;
    }

    Node cycleNode() {
        if ( shared.cycle_found ) {
            assert( shared.cycle_node.valid() );
            return shared.cycle_node;
        }

        for ( int i = 0; i < shareds.size(); ++i ) {
            if ( shareds[ i ].cycle_found ) {
                assert( shareds[ i ].cycle_node.valid() );
                return shareds[ i ].cycle_node;
            }
        }

        assert_die();
    }

    int totalSize() {
        int sz = 0;
        for ( int i = 0; i < shareds.size(); ++i )
            sz += shareds[ i ].size;
        return sz;
    }

    template< typename V >
    void queueAll( V &v, bool reset = false ) {
        for ( size_t i = 0; i < this->table().size(); ++i ) {
            Node st = this->table()[ i ];
            if ( st.valid() ) {
                if ( reset )
                    extension( st ).predCount = 0;
                if ( extension( st ).inS && extension( st ).inF ) {
                    assert_eq( v.owner( st ), v.worker.id() );
                    v.queueAny( Blob(), st ); // slightly faster maybe
                }
            }
        }
    }

    VertexId makeId( Node t ) {
        VertexId r;
        r.ptr = reinterpret_cast< uintptr_t >( t.ptr ) >> 2;
        r.owner = this->id();
        return r;
    }

    VertexId getMap( Node t ) {
        VertexId r;
        r.ptr = extension( t ).map;
        r.owner = extension( t ).map_owner;
        return r;
    }

    void setMap( Node t, VertexId id ) {
        extension( t ).map = id.ptr;
        extension( t ).map_owner = id.owner;
    }

    bool updateIteration( Node t ) {
        int old = extension( t ).iteration;
        extension( t ).iteration = shared.iteration;
        return old != shared.iteration;
    }

    // -----------------------------------------------
    // -- REACHABILITY implementation
    // --

    visitor::ExpansionAction reachExpansion( Node st )
    {
        assert( extension( st ).predCount > 0 );
        assert( extension( st ).inS );
        ++ shared.size;
        shared.stats.addExpansion();
        return visitor::ExpandState;
    }

    visitor::TransitionAction reachTransition( Node f, Node t )
    {
        ++ extension( t ).predCount;
        extension( t ).inS = true;
        if ( extension( t ).inF && f.valid() )
            return visitor::ForgetTransition;
        else {
            return updateIteration( t ) ?
                visitor::ExpandTransition :
                visitor::ForgetTransition;
        }
    }

    void _reachability() { // parallel
        typedef visitor::Setup< G, This, Table, Statistics,
            &This::reachTransition,
            &This::reachExpansion > Setup;
        typedef visitor::Partitioned< Setup, This, Hasher > Visitor;

        Visitor visitor( m_graph, *this, *this, hasher, &this->table() );
        queueAll( visitor, true );
        visitor.processQueue();
    }

    void reachability() {
        shared.size = 0;
        parallel( &This::_reachability );
        shared.size = totalSize();
    }

    // -----------------------------------------------
    // -- INITIALISE implementation
    // --

    visitor::TransitionAction initTransition( Node from, Node to )
    {
        if ( !extension( to ).parent.valid() )
            extension( to ).parent = from;
        if ( from.valid() ) {
            VertexId fromMap = getMap( from );
            VertexId fromId = makeId( from );
            if ( getMap( to ) < fromMap )
                setMap( to, fromMap  );
            if ( m_graph.isAccepting( from ) ) {
                if ( from.pointer() == to.pointer() ) {
                    shared.cycle_node = to;
                    shared.cycle_found = true;
                    return visitor::TerminateOnTransition;
                }
                if ( getMap( to ) < fromId )
                    setMap( to, fromId );
            }
            if ( makeId( to ) == fromMap ) {
                shared.cycle_node = to;
                shared.cycle_found = true;
                return visitor::TerminateOnTransition;
            }
        }
        shared.stats.addEdge();
        m_graph.porTransition( from, to, &updatePredCount );
        return visitor::FollowTransition;
    }

    visitor::ExpansionAction initExpansion( Node st )
    {
        extension( st ).inF = extension( st ).inS = m_graph.isAccepting( st );
        shared.size += extension( st ).inS;
        shared.stats.addNode( m_graph, st );
        m_graph.porExpansion( st );
        return visitor::ExpandState;
    }

    void _initialise() { // parallel
        typedef visitor::Setup< G, This, Table, Statistics,
            &This::initTransition,
            &This::initExpansion > Setup;
        typedef visitor::Partitioned< Setup, This, Hasher > Visitor;

        this->comms().notify( this->id(), &m_graph.pool() );
        Visitor visitor( m_graph, *this, *this, hasher, &this->table() );
        m_graph.queueInitials( visitor );
        visitor.processQueue();
    }

    void initialise() {
        parallel( &This::_initialise ); updateResult();
        shared.oldsize = shared.size = totalSize();
        while ( true ) {
            if ( cycleFound() ) {
                result().fullyExplored = meta::Result::No;
                return;
            }

            shared.need_expand = false;
            ring< This >( &This::_por );
            updateResult();

            if ( shared.need_expand ) {
                parallel( &This::_initialise );
                updateResult();
            } else
                break;
        };
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

    // -----------------------------------------------
    // -- ELIMINATION implementation 
    // --

    visitor::ExpansionAction elimExpansion( Node st )
    {
        assert( extension( st ).predCount == 0 );
        extension( st ).inS = false;
        if ( extension( st ).inF )
            shared.size ++;
        shared.stats.addExpansion();
        return visitor::ExpandState;
    }

    visitor::TransitionAction elimTransition( Node f, Node t )
    {
        assert( t.valid() );
        assert( extension( t ).inS );
        assert( extension( t ).predCount >= 1 );
        -- extension( t ).predCount;
        // we follow a transition only if the target state is going to
        // be eliminated
        if ( extension( t ).predCount == 0 ) {
            return updateIteration( t ) ?
                visitor::ExpandTransition :
                visitor::ForgetTransition;
        } else
            return visitor::ForgetTransition;
    }

    void _elimination() {
        typedef visitor::Setup< G, This, Table, Statistics,
            &This::elimTransition,
            &This::elimExpansion > Setup;
        typedef visitor::Partitioned< Setup, This, Hasher > Visitor;

        Visitor visitor( m_graph, *this, *this, hasher, &this->table() );
        queueAll( visitor );
        visitor.processQueue();
    }

    void elimination() {
        shared.size = 0;
        parallel( &This::_elimination );
        shared.oldsize = shared.size = shared.oldsize - totalSize();
    }


    // -------------------------------------------
    // -- COUNTEREXAMPLES
    // --

    visitor::TransitionAction ccTransition( Node from, Node to )
    {
        if ( !extension( to ).inS )
            return visitor::ForgetTransition;
        if ( from.valid() && to == shared.cycle_node ) {
            shared.cycle_found = true;
            return visitor::TerminateOnTransition;
        }

        return updateIteration( to ) ?
            visitor::ExpandTransition :
            visitor::ForgetTransition;
    }

    visitor::ExpansionAction ccExpansion( Node ) {
        return visitor::ExpandState;
    }

    void _checkCycle() {
        typedef visitor::Setup< G, This, Table, Statistics,
            &This::ccTransition,
            &This::ccExpansion > Setup;
        typedef visitor::Partitioned< Setup, This, Hasher > Visitor;

        Visitor visitor( m_graph, *this, *this, hasher, &this->table() );
        assert( shared.cycle_node.valid() );
        visitor.queue( Blob(), shared.cycle_node );
        visitor.processQueue();
    }

    Shared _counterexample( Shared sh ) {
        shared = sh;
        for ( int i = 0; i < this->table().size(); ++i ) {
            if ( cycleFound() ) {
                shared.cycle_node = cycleNode();
                shared.cycle_found = true;
                return shared;
            }
            Node st = shared.cycle_node = this->table()[ i ];
            if ( !st.valid() )
                continue;
            if ( extension( st ).iteration == shared.iteration )
                continue; // already seen
            if ( !extension( st ).inS || !extension( st ).inF )
                continue;
            parallel( &This::_checkCycle );
        }
        return shared;
    }

    Shared _parentTrace( Shared sh ) {
        shared = sh;
        ce.setup( m_graph, shared ); // XXX this will be done many times needlessly
        ce._parentTrace( *this, hasher, equal, this->table() );
        return shared;
    }

    void _traceCycle() {
        ce._traceCycle( *this, hasher, this->table() );
    }

    void counterexample() {
        if ( !cycleFound() ) {
            progress() << " obtaining counterexample...      " << std::flush;
            ring( &This::_counterexample );
            progress() << "done" << std::endl;
        }

        progress() << " generating counterexample...      " << std::flush;
        assert( cycleFound() );
        shared.ce.initial = cycleNode();

        ce.setup( m_graph, shared );
        ce.lasso( *this, *this );
        progress() << "done" << std::endl;
    }

    // -------------------------------------------
    // -- MAIN LOOP implementation
    // --

    void printSize() {
        if ( cycleFound() )
            progress() << "MAP/ET" << std::endl << std::flush;
        else
            progress() << "|S| = " << shared.size << std::endl << std::flush;
    }

    void printIteration( int i ) {
        progress() << "------------- iteration " << i
                   << " ------------- " << std::endl;
    }

    void updateResult() {
        for ( int i = 0; i < shareds.size(); ++i ) {
            shared.stats.merge( shareds[ i ].stats );
            shareds[ i ].stats = algorithm::Statistics();
        }
        shared.stats.update( meta().statistics );
        shared.stats = algorithm::Statistics();
    }

    void run()
    {
        size_t oldsize = 0;

        result().fullyExplored = meta::Result::Yes;
        progress() << " initialise...\t\t    " << std::flush;
        shared.size = 0;
        initialise();
        printSize();

        shared.iteration = 1;

        if ( !cycleFound() ) {
            do {
                oldsize = shared.size;

                printIteration( 1 + shared.iteration / 2 );

                progress() << " reachability...\t    " << std::flush;
                reachability();
                printSize(); updateResult();

                ++shared.iteration;

                progress() << " elimination & reset...\t    " << std::flush;
                elimination();
                printSize(); updateResult();

                ++shared.iteration;

            } while ( oldsize != shared.size && shared.size != 0 );
        }

        bool valid = cycleFound() ? false : ( shared.size == 0 );
        livenessBanner( valid );

        if ( want_ce && !valid ) {
            counterexample();
            result().ceType = meta::Result::Cycle;
        }

        result().propertyHolds = valid ? meta::Result::Yes : meta::Result::No;
        meta().statistics.deadlocks = -1; /* did not count */
    }

    Owcty( Meta m, bool master = false )
        : Algorithm( m, sizeof( Extension ) )
    {
        shared.size = 0;
        if ( master )
            this->becomeMaster( m.execution.threads, m );
        this->init( this );
    }
};

ALGORITHM_RPC( Owcty );
ALGORITHM_RPC_ID( Owcty, 1, _initialise );
ALGORITHM_RPC_ID( Owcty, 2, _reachability );
ALGORITHM_RPC_ID( Owcty, 3, _elimination );
ALGORITHM_RPC_ID( Owcty, 4, _counterexample );
ALGORITHM_RPC_ID( Owcty, 5, _checkCycle );
ALGORITHM_RPC_ID( Owcty, 6, _parentTrace );
ALGORITHM_RPC_ID( Owcty, 7, _traceCycle );
ALGORITHM_RPC_ID( Owcty, 8, _por );
ALGORITHM_RPC_ID( Owcty, 9, _por_worker );

}
}

#endif
