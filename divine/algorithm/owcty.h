// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/algorithm/map.h> // for MapVertexId
#include <divine/graph/ltlce.h>

#ifndef DIVINE_ALGORITHM_OWCTY_H
#define DIVINE_ALGORITHM_OWCTY_H

namespace divine {
namespace algorithm {

template < typename Handle >
struct OwctyShared {
    int64_t size, oldsize;
    Handle cycle_node;
    bool cycle_found;
    int iteration;
    CeShared< Blob, Handle > ce;
    algorithm::Statistics stats;
    bool need_expand;

    OwctyShared() : cycle_found( false ) {}
};

template< typename BS, typename Handle >
typename BS::bitstream &operator<<( BS &bs, const OwctyShared< Handle > &sh )
{
    return bs << sh.size << sh.oldsize << sh.cycle_node << sh.cycle_found
              << sh.iteration << sh.ce << sh.stats << sh.need_expand;
}

template< typename BS, typename Handle >
typename BS::bitstream &operator>>( BS &bs, OwctyShared< Handle > &sh )
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
struct Owcty : Algorithm, AlgorithmUtils< Setup, OwctyShared< typename Setup::Store::Handle > >,
               Parallel< Setup::template Topology, Owcty< Setup > >
{
    using This = Owcty< Setup >;
    using Shared = OwctyShared< typename Setup::Store::Handle >;
    using Utils = AlgorithmUtils< Setup, Shared >;

    ALGORITHM_CLASS( Setup );
    BRICK_RPC( Utils, &This::_initialise, &This::_reachability, &This::_elimination,
                      &This::_counterexample, &This::_checkCycle,
                      &This::_parentTrace, &This::_traceCycle,
                      &This::_por, &This::_por_worker,
                      &This::_successorTrace, &This::_ceIsInitial );

    // -------------------------------
    // -- Some useful types
    // --

    struct Extension {
        BitTuple<
            BitField< Handle >,
            BitField< MapVertexId >,
            BitLock,
            BitField< bool, 1 >,
            BitField< bool, 1 >,
            BitField< size_t, 19 >,// up to 1M predecessors per node
            BitField< size_t, 10 > // handle up to 1024 iterations
        > data;

        void lock() { get< 2 >( data ).lock(); }
        void unlock() { get< 2 >( data ).unlock(); }

        auto parent() -> decltype( get< 0 >( data ) ) {
            return get< 0 >( data );
        }
        auto map() -> decltype( get< 1 >( data ) ) {
            return get< 1 >( data );
        }

        auto inS() -> decltype( get< 3 >( data ) ) {
            return get< 3 >( data );
        }
        auto inF() -> decltype( get< 4 >( data ) ) {
            return get< 4 >( data );
        }
        auto predCount() -> decltype( get< 5 >( data ) ) {
            return get< 5 >( data );
        }
        auto iteration() -> decltype( get< 6 >( data ) ) {
            return get< 6 >( data );
        }
    };

    using Guard = typename Store::template Guard< Extension >;

    typedef LtlCE< Setup, Shared, Extension, Hasher > CE;
    CE ce;

    // ------------------------------------------------------
    // -- generally useful utilities
    // --

    using Utils::store;
    using Utils::shared;
    using Utils::shareds;

    Pool& pool() {
        return this->graph().pool();
    }

    Extension &extension( Vertex v ) {
        return v.template extension< Extension >();
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

    Handle cycleNode() {
        if ( shared.cycle_found ) {
            ASSERT( this->store().valid( shared.cycle_node ) );
            return shared.cycle_node;
        }

        for ( int i = 0; i < int( shareds.size() ); ++i ) {
            if ( shareds[ i ].cycle_found ) {
                ASSERT( store().valid( shareds[ i ].cycle_node ) );
                return shareds[ i ].cycle_node;
            }
        }

        ASSERT_UNREACHABLE( "cycle node not found" );
    }

    int totalSize() {
        int sz = 0;
        for ( int i = 0; i < int( shareds.size() ); ++i )
            sz += shareds[ i ].size;
        return sz;
    }

    template< typename V >
    void queueAll( V &visitor, bool reset = false ) {
        for ( auto st : this->store() ) {
            if ( reset )
                extension( st ).predCount() = 0;
            if ( extension( st ).inS() && extension( st ).inF() ) {
                visitor.queue( Vertex(), st.node(), Label() ); // slightly faster maybe
                st.disown();
            }
        }
    }

    MapVertexId makeId( Vertex t ) {
        return MapVertexId( t.handle() );
    }

    MapVertexId getMap( Vertex t ) {
        return extension( t ).map();
    }

    void setMap( Vertex t, MapVertexId id ) {
        extension( t ).map() = id;
    }

    bool updateIteration( Vertex t ) {
        int old = extension( t ).iteration();
        extension( t ).iteration() = shared.iteration;
        return old != shared.iteration;
    }

    struct Reachability : Visit< This, Setup >
    {
        static visitor::ExpansionAction expansion( This &o, Vertex st )
        {
            Guard guard( st );
            ASSERT( o.extension( st ).predCount() > 0 );
            ASSERT( o.extension( st ).inS() );
            ++ o.shared.size;
            o.shared.stats.addExpansion();
            return visitor::ExpansionAction::Expand;
        }

        static visitor::TransitionAction transition( This &o, Vertex f, Vertex t, Label )
        {
            Guard guard( t );
            ++ o.extension( t ).predCount();
            o.extension( t ).inS() = true;
            if ( o.extension( t ).inF() && o.store().valid( f ) )
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
            Guard guard( from, to );
            if ( !o.store().valid( o.extension( to ).parent() ) )
                o.extension( to ).parent() = from.handle();
            if ( o.store().valid( from ) ) {
                MapVertexId fromMap = o.getMap( from );
                MapVertexId fromId = o.makeId( from );
                if ( o.getMap( to ) < fromMap )
                    o.setMap( to, fromMap  );
                if ( o.graph().stateFlags( from.node(), graph::flags::isAccepting ) ) {
                    if ( from.handle().asNumber() == to.handle().asNumber() ) { // hmm
                        o.shared.cycle_node = to.handle();
                        o.shared.cycle_found = true;
                        return visitor::TransitionAction::Terminate;
                    }
                    if ( o.getMap( to ) < fromId )
                        o.setMap( to, fromId );
                }
                if ( o.makeId( to ) == fromMap ) {
                    o.shared.cycle_node = to.handle();
                    o.shared.cycle_found = true;
                    return visitor::TransitionAction::Terminate;
                }
            }
            o.shared.stats.addEdge( o.store(), from, to );
            o.graph().porTransition( o.store(), from, to );
            return visitor::TransitionAction::Follow;
        }

        static visitor::ExpansionAction expansion( This &o, Vertex st )
        {
            Guard guard( st );
            o.extension( st ).inF()
                = o.extension( st ).inS()
                = !!o.graph().stateFlags( st.node(), graph::flags::isAccepting );
            o.shared.size += o.extension( st ).inS();
            o.shared.stats.addNode( o.graph(), st );
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
                result().fullyExplored = meta::Result::R::No;
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
        this->graph()._porEliminate( *this );
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
            Guard guard( st );
            ASSERT( o.extension( st ).predCount() == 0 );
            o.extension( st ).inS() = false;
            if ( o.extension( st ).inF() )
                o.shared.size ++;
            o.shared.stats.addExpansion();
            return visitor::ExpansionAction::Expand;
        }

        static visitor::TransitionAction transition( This &o, Vertex, Vertex t, Label )
        {
            Guard guard( t );
            ASSERT( o.store().valid( t ) );
            ASSERT( o.extension( t ).inS() );
            ASSERT( o.extension( t ).predCount() >= 1 );
            -- o.extension( t ).predCount();
            // we follow a transition only if the target state is going to
            // be eliminated
            if ( o.extension( t ).predCount() == 0 ) {
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
        ASSERT_LEQ( 0, shared.size );
    }

    struct FindCE : Visit< This, Setup > {
        static visitor::TransitionAction transition( This &o, Vertex from, Vertex to, Label )
        {
            Guard guard( from, to );
            if ( !o.extension( to ).inS() )
                return visitor::TransitionAction::Forget;

            if ( o.store().valid( from ) &&
                 o.store().equal( to.handle(), o.shared.cycle_node ) )
            {
                o.shared.cycle_found = true;
                return visitor::TransitionAction::Terminate;
            }

            return o.updateIteration( to ) ?
                visitor::TransitionAction::Expand :
                visitor::TransitionAction::Forget;
        }

        template< typename V > void queueInitials( This &o, V &v ) {
            if ( o.store().knows( o.shared.cycle_node ) ) {
                Vertex c = o.store().vertex( o.shared.cycle_node );
                v.queue( Vertex(), c.node(), Label() );
                c.disown();
            }
        }
    };

    void _checkCycle() {
        ASSERT( this->store().valid( shared.cycle_node ) );
        this->visit( this, FindCE() );
    }

    Shared _counterexample( Shared sh ) {
        if ( sh.cycle_found ) /* shortcut */
            return sh;

        shared = sh;
        for ( auto st : this->store() ) {
            shared.cycle_node = st.handle();
            if ( !this->store().valid( st ) )
                continue;
            if ( int( extension( st ).iteration() ) == shared.iteration )
                continue; // already seen
            if ( !extension( st ).inS() || !extension( st ).inF() )
                continue;
            parallel( &This::_checkCycle );
            if ( cycleFound() ) {
                shared.cycle_node = cycleNode();
                shared.cycle_found = true;
                return shared;
            }
        }
        return shared;
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

    void counterexample() {
        if ( !cycleFound() ) {
            progress() << " obtaining counterexample...      " << std::flush;
            ring( &This::_counterexample );
            progress() << "done" << std::endl;
        }

        progress() << " generating counterexample...      " << std::flush;
        ASSERT( cycleFound() );
        shared.ce.initial = cycleNode();

        ce.setup( *this, shared );
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
        int64_t oldsize = 0;

        result().fullyExplored = meta::Result::R::Yes;
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
            result().ceType = meta::Result::CET::Cycle;
        }

        result().propertyHolds = valid ? meta::Result::R::Yes : meta::Result::R::No;
        meta().statistics.deadlocks = -1; /* did not count */
    }

    Owcty( Meta m ) : Algorithm( m, sizeof( Extension ) )
    {
        this->init( *this );
    }

    Owcty( Owcty &master, std::pair< int, int > id ) : Algorithm( master.meta(), sizeof( Extension ) )
    {
        this->init( *this, master, id );
    }
};

}
}

#endif
