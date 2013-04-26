// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/algorithm/map.h> // for MapVertexId
#include <divine/graph/ltlce.h>

#ifndef DIVINE_ALGORITHM_OWCTY_H
#define DIVINE_ALGORITHM_OWCTY_H

namespace divine {
namespace algorithm {

template < typename VertexId >
struct OwctyShared {
    size_t size, oldsize;
    VertexId cycle_node;
    bool cycle_found;
    int iteration;
    CeShared< Blob, VertexId > ce;
    algorithm::Statistics stats;
    bool need_expand;

    OwctyShared() : cycle_found( false ) {}
};

template< typename BS, typename VertexId >
typename BS::bitstream &operator<<( BS &bs, const OwctyShared< VertexId > &sh )
{
    return bs << sh.size << sh.oldsize << sh.cycle_node << sh.cycle_found
              << sh.iteration << sh.ce << sh.stats << sh.need_expand;
}

template< typename BS, typename VertexId >
typename BS::bitstream &operator>>( BS &bs, OwctyShared< VertexId > &sh )
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
template< typename Setup >
struct Owcty : Algorithm, AlgorithmUtils< Setup >, Parallel< Setup::template Topology, Owcty< Setup > >
{
    typedef Owcty< Setup > This;
    ALGORITHM_CLASS( Setup, OwctyShared< typename Setup::VertexId > );

    // -------------------------------
    // -- Some useful types
    // --

    struct Extension {
        VertexId parent;
        uintptr_t map:(8 * sizeof(uintptr_t) - 2);
        bool inS:1;
        bool inF:1;

        // ... the last word is a bit crammed, so consider just extending this
        size_t predCount:14; // up to 16k predecessors per node
        size_t iteration:9; // handle up to 512 iterations
        unsigned map_owner:9; // handle up to 512 MPI nodes
    };

    typedef LtlCE< Setup, Shared, Extension, Hasher > CE;
    CE ce;

    // ------------------------------------------------------
    // -- generally useful utilities
    // --

    using AlgorithmUtils< Setup >::store;
    Pool& pool() {
        return this->graph().pool();
    }

    static Extension &extension( Pool pool, VertexId n ) {
        return n.template extension< Extension >( pool );
    }

    Extension &extension( Vertex n ) {
        return extension( n.getVertexId() );
    }

    Extension &extension( VertexId id ) {
        return id.template extension< Extension >( pool() );
    }

    static void updatePredCount( Pool& pool, VertexId n, int p ) {
        extension( pool, n ).predCount = p;
    }

    bool cycleFound() {
        if ( shared.cycle_found )
            return true;
        for ( int i = 0; i < int( shareds.size() ); ++i ) {
            if ( shareds[ i ].cycle_found )
                return true;
        }
        return false;
    }

    VertexId cycleNode() {
        if ( shared.cycle_found ) {
            assert( this->store().valid( shared.cycle_node ) );
            return shared.cycle_node;
        }

        for ( int i = 0; i < int( shareds.size() ); ++i ) {
            if ( shareds[ i ].cycle_found ) {
                assert( store().valid( shareds[ i ].cycle_node ) );
                return shareds[ i ].cycle_node;
            }
        }

        assert_die();
    }

    int totalSize() {
        int sz = 0;
        for ( int i = 0; i < int( shareds.size() ); ++i )
            sz += shareds[ i ].size;
        return sz;
    }

    template< typename V >
    void queueAll( V &visitor, bool reset = false ) {
        for ( VertexId st : this->store() ) {
            if ( this->store().valid( st ) ) {
                if ( reset )
                    extension( st ).predCount = 0;
                if ( extension( st ).inS && extension( st ).inF ) {
                    assert_eq( store().owner( visitor.worker, st ), visitor.worker.id() );
                    Vertex v = store().fetchByVertexId( st );
                    visitor.queueAny( Vertex(), v.getNode(), Label() ); // slightly faster maybe
                }
            }
        }
    }

    MapVertexId makeId( Vertex t ) {
        MapVertexId r;
        r.ptr = t.getVertexId().weakId() >> 2;
        r.owner = this->id();
        return r;
    }

    MapVertexId getMap( Vertex t ) {
        MapVertexId r;
        r.ptr = extension( t ).map;
        r.owner = extension( t ).map_owner;
        return r;
    }

    void setMap( Vertex t, MapVertexId id ) {
        extension( t ).map = id.ptr;
        extension( t ).map_owner = id.owner;
    }

    bool updateIteration( Vertex t ) {
        int old = extension( t ).iteration;
        extension( t ).iteration = shared.iteration;
        return old != shared.iteration;
    }

    struct Reachability : Visit< This, Setup >
    {
        static visitor::ExpansionAction expansion( This &o, Vertex st )
        {
            assert( o.extension( st ).predCount > 0 );
            assert( o.extension( st ).inS );
            ++ o.shared.size;
            o.shared.stats.addExpansion();
            return visitor::ExpansionAction::Expand;
        }

        static visitor::TransitionAction transition( This &o, Vertex f, Vertex t, Label )
        {
            ++ o.extension( t ).predCount;
            o.extension( t ).inS = true;
            if ( o.extension( t ).inF && o.store().valid( f ) )
                return visitor::TransitionAction::Forget;
            else {
                return o.updateIteration( t ) ?
                    visitor::TransitionAction::Expand :
                    visitor::TransitionAction::Forget;
            }
        }

        template< typename Visitor >
        void queueInitials( This &t, Visitor &v ) {
            t.queueAll( v, true );
        }
    };

    void _reachability() { // parallel
        this->visit( this, Reachability() );
    }

    void reachability() {
        shared.size = 0;
        parallel( &This::_reachability );
        shared.size = totalSize();
    }

    struct Initialise : Visit< This, Setup >
    {
        static visitor::TransitionAction transition( This &o, Vertex from, Vertex to, Label )
        {
            if ( !o.store().valid( o.extension( to ).parent ) )
                o.extension( to ).parent = from.getVertexId();
            if ( o.store().valid( from ) ) {
                MapVertexId fromMap = o.getMap( from );
                MapVertexId fromId = o.makeId( from );
                if ( o.getMap( to ) < fromMap )
                    o.setMap( to, fromMap  );
                if ( o.graph().isAccepting( from.getNode() ) ) {
                    if ( from.getVertexId().weakId() == to.getVertexId().weakId() ) { // hmm
                        o.shared.cycle_node = to.getVertexId();
                        o.shared.cycle_found = true;
                        return visitor::TransitionAction::Terminate;
                    }
                    if ( o.getMap( to ) < fromId )
                        o.setMap( to, fromId );
                }
                if ( o.makeId( to ) == fromMap ) {
                    o.shared.cycle_node = to.getVertexId();
                    o.shared.cycle_found = true;
                    return visitor::TransitionAction::Terminate;
                }
            }
            o.shared.stats.addEdge( o.graph(), from.getNode(), to.getNode() );
            int predCount = o.extension( to ).predCount;
            o.graph().porTransition( from, to, &predCount );
            o.extension( to ).predCount = predCount;
            return visitor::TransitionAction::Follow;
        }

        static visitor::ExpansionAction expansion( This &o, Vertex st )
        {
            o.extension( st ).inF = o.extension( st ).inS = o.graph().isAccepting( st.getNode() );
            o.shared.size += o.extension( st ).inS;
            o.shared.stats.addNode( o.graph(), st.getNode() );
            return visitor::ExpansionAction::Expand;
        }
    };

    void _initialise() { // parallel
        this->visit( this, Initialise() );
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
        this->graph()._porEliminate( *this, &updatePredCount );
    }

    Shared _por( Shared sh ) {
        shared = sh;
        if ( this->graph().porEliminate( *this, *this ) )
            shared.need_expand = true;
        return shared;
    }

    struct Elimination : Visit< This, Setup >
    {
        static visitor::ExpansionAction expansion( This &o, Vertex st )
        {
            assert( o.extension( st ).predCount == 0 );
            o.extension( st ).inS = false;
            if ( o.extension( st ).inF )
                o.shared.size ++;
            o.shared.stats.addExpansion();
            return visitor::ExpansionAction::Expand;
        }

        static visitor::TransitionAction transition( This &o, Vertex f, Vertex t, Label )
        {
            assert( o.store().valid( t ) );
            assert( o.extension( t ).inS );
            assert( o.extension( t ).predCount >= 1 );
            -- o.extension( t ).predCount;
            // we follow a transition only if the target state is going to
            // be eliminated
            if ( o.extension( t ).predCount == 0 ) {
                return o.updateIteration( t ) ?
                    visitor::TransitionAction::Expand :
                    visitor::TransitionAction::Forget;
            } else
                return visitor::TransitionAction::Forget;
        }

        template< typename Visitor >
        void queueInitials( This &t, Visitor &v ) {
            t.queueAll( v );
        }
    };

    void _elimination() {
        this->visit( this, Elimination() );
    }

    void elimination() {
        shared.size = 0;
        parallel( &This::_elimination );
        shared.oldsize = shared.size = shared.oldsize - totalSize();
    }

    struct FindCE : Visit< This, Setup > {
        static visitor::TransitionAction transition( This &o, Vertex from, Vertex to, Label )
        {
            if ( !o.extension( to ).inS )
                return visitor::TransitionAction::Forget;
            if ( o.store().valid( from ) &&
                    visitor::equalId( o.store(), to.getVertexId(), o.shared.cycle_node ) ) {
                o.shared.cycle_found = true;
                return visitor::TransitionAction::Terminate;
            }

            return o.updateIteration( to ) ?
                visitor::TransitionAction::Expand :
                visitor::TransitionAction::Forget;
        }
/* Not called from anywhere
        static visitor::ExpansionAction ccExpansion( Node ) {
            return visitor::ExpandState;
        }
*/
        template< typename V > void queueInitials( This &o, V &v ) {
            Vertex c = o.store().fetchByVertexId( o.shared.cycle_node );
            v.queue( Vertex(), c.getNode(), Label() );
        }
    };

    void _checkCycle() {
        assert( this->store().valid( shared.cycle_node ) );
        this->visit( this, FindCE() );
    }

    Shared _counterexample( Shared sh ) {
        shared = sh;
        for ( VertexId st : this->store() ) {
            if ( cycleFound() ) {
                shared.cycle_node = cycleNode();
                shared.cycle_found = true;
                return shared;
            }
            shared.cycle_node = st;
            if ( !this->store().valid( st ) )
                continue;
            if ( extension( st ).iteration == shared.iteration )
                continue; // already seen
            if ( !extension( st ).inS || !extension( st ).inF )
                continue;
            parallel( &This::_checkCycle );
        }
        return shared;
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

    void _traceCycle() {
        ce._traceCycle( *this );
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

        ce.setup( this->graph(), shared, this->store().hasher() );
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
        progress() << " ---------------- iteration " << i
                   << " ---------------- " << std::endl;
    }

    void updateResult() {
        for ( int i = 0; i < int( shareds.size() ); ++i ) {
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
        resultBanner( valid );

        if ( want_ce && !valid ) {
            counterexample();
            result().ceType = meta::Result::Cycle;
        }

        result().propertyHolds = valid ? meta::Result::Yes : meta::Result::No;
        meta().statistics.deadlocks = -1; /* did not count */
    }

    Owcty( Meta m ) : Algorithm( m, sizeof( Extension ) )
    {
        this->init( this );
        this->becomeMaster( m.execution.threads, this );
    }

    Owcty( Owcty *master ) : Algorithm( master->meta(), sizeof( Extension ) )
    {
        this->init( this, master );
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
ALGORITHM_RPC_ID( Owcty, 10, _successorTrace );
ALGORITHM_RPC_ID( Owcty, 11, _ceIsInitial );

}
}

#endif
