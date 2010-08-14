// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/algorithm/map.h> // for VertexId
#include <divine/ltlce.h>
#include <divine/visitor.h>
#include <divine/parallel.h>
#include <divine/report.h>

#ifndef DIVINE_OWCTY_H
#define DIVINE_OWCTY_H

namespace divine {
namespace algorithm {

template< typename, typename > struct Owcty;

// ------------------------------------------
// -- Some drudgery for MPI's sake
// --
template< typename G, typename S >
struct _MpiId< Owcty< G, S > >
{
    typedef Owcty< G, S > A;

    static void (A::*from_id( int n ))()
    {
        switch ( n ) {
            case 0: return &A::_initialise;
            case 1: return &A::_reachability;
            case 2: return &A::_elimination;
            case 3: return &A::_counterexample;
            case 4: return &A::_checkCycle;
            case 5: return &A::_parentTrace;
            case 6: return &A::_traceCycle;
            case 7: return &A::_por;
            case 8: return &A::_por_worker;
            default: assert_die();
        }
    }

    static int to_id( void (A::*f)() ) {
        if( f == &A::_initialise ) return 0;
        if( f == &A::_reachability ) return 1;
        if( f == &A::_elimination ) return 2;
        if( f == &A::_counterexample ) return 3;
        if( f == &A::_checkCycle) return 4;
        if( f == &A::_parentTrace) return 5;
        if( f == &A::_traceCycle) return 6;
        if( f == &A::_por) return 7;
        if( f == &A::_por_worker) return 8;
        assert_die();
    }

    template< typename O >
    static void writeShared( typename A::Shared s, O o ) {
        o = s.stats.write( o );
        *o++ = s.need_expand;
        *o++ = s.initialTable;
        *o++ = s.size;
        *o++ = s.oldsize;
        *o++ = s.iteration;
        *o++ = s.cycle_found;
        *o++ = s.cycle_node.valid();
        if ( s.cycle_node.valid() )
            o = s.cycle_node.write32( o );
        o = s.ce.write( o );
    }

    template< typename I >
    static I readShared( typename A::Shared &s, I i ) {
        i = s.stats.read( i );
        s.need_expand = *i++;
        s.initialTable = *i++;
        s.size = *i++;
        s.oldsize = *i++;
        s.iteration = *i++;
        s.cycle_found = *i++;
        bool valid = *i++;
        if ( valid ) {
            FakePool fp;
            i = s.cycle_node.read32( &fp, i );
        }
        i = s.ce.read( i );
        return i;
    }
};

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
template< typename G, typename Statistics >
struct Owcty : Algorithm, DomainWorker< Owcty< G, Statistics > >
{
    typedef Owcty< G, Statistics > This;
    typedef typename G::Node Node;

    // -------------------------------
    // -- Some useful types
    // --

    struct Shared {
        size_t size, oldsize;
        Node cycle_node;
        bool cycle_found;
        int iteration;
        G g;
        CeShared< Node > ce;
        int initialTable;
        algorithm::Statistics< G > stats;
        bool need_expand;

        Shared() : cycle_found( false ) {}
    } shared;

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

    Domain< This > &domain() {
        return DomainWorker< This >::domain();
    }

    static Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    static void updatePredCount( Node n, int p ) {
        extension( n ).predCount = p;
    }

    bool cycleFound() {
        if ( shared.cycle_found )
            return true;
        for ( int i = 0; i < domain().peers(); ++i ) {
            if ( domain().shared( i ).cycle_found )
                return true;
        }
        return false;
    }

    Node cycleNode() {
        if ( shared.cycle_found ) {
            assert( shared.cycle_node.valid() );
            return shared.cycle_node;
        }

        for ( int i = 0; i < domain().peers(); ++i ) {
            if ( domain().shared( i ).cycle_found ) {
                assert( domain().shared( i ).cycle_node.valid() );
                return domain().shared( i ).cycle_node;
            }
        }

        assert_die();
    }

    int totalSize() {
        int sz = 0;
        for ( int i = 0; i < domain().peers(); ++i ) {
            Shared &s = domain().shared( i );
            sz += s.size;
        }
        return sz;
    }

    void resetSize() {
        shared.size = 0;
        for ( int i = 0; i < domain().peers(); ++i ) {
            Shared &s = domain().shared( i );
            s.size = 0;
        }
    }

    template< typename V >
    void queueAll( V &v, bool reset = false ) {
        for ( size_t i = 0; i < table().size(); ++i ) {
            Node st = table()[ i ].key;
            if ( st.valid() ) {
                if ( reset )
                    extension( st ).predCount = 0;
                if ( extension( st ).inS && extension( st ).inF ) {
                    assert_eq( v.owner( st ), v.worker.globalId() );
                    v.queueAny( Blob(), st ); // slightly faster maybe
                }
            }
        }
    }

    VertexId makeId( Node t ) {
        VertexId r;
        r.ptr = reinterpret_cast< uintptr_t >( t.ptr ) >> 2;
        r.owner = this->globalId();
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
        typedef visitor::Parallel< Setup, This, Hasher > Visitor;

        Visitor visitor( shared.g, *this, *this, hasher, &table() );
        queueAll( visitor, true );
        visitor.processQueue();
    }

    void reachability() {
        shared.size = 0;
        domain().parallel().run( shared, &This::_reachability );
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
            if ( shared.g.isAccepting( from ) ) {
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
        shared.g.porTransition( from, to, &updatePredCount );
        return visitor::FollowTransition;
    }

    visitor::ExpansionAction initExpansion( Node st )
    {
        extension( st ).inF = extension( st ).inS = shared.g.isAccepting( st );
        shared.size += extension( st ).inS;
        shared.stats.addNode( shared.g, st );
        shared.g.porExpansion( st );
        return visitor::ExpandState;
    }

    void _initialise() { // parallel
        typedef visitor::Setup< G, This, Table, Statistics,
            &This::initTransition,
            &This::initExpansion > Setup;
        typedef visitor::Parallel< Setup, This, Hasher > Visitor;

        m_initialTable = &shared.initialTable; // XXX find better place for this
        Visitor visitor( shared.g, *this, *this, hasher, &table() );
        shared.g.queueInitials( visitor );
        visitor.processQueue();
    }

    void initialise() {
        domain().parallel().run( shared, &This::_initialise ); updateResult();
        shared.oldsize = shared.size = totalSize();
        while ( true ) {
            if ( cycleFound() ) {
                result().fullyExplored = Result::No;
                return;
            }

            shared.need_expand = false;
            domain().parallel().runInRing( shared, &This::_por );
            updateResult();

            if ( shared.need_expand ) {
                domain().parallel().run( shared, &This::_initialise );
                updateResult();
            } else
                break;
        };
    }

    void _por_worker() {
        shared.g._porEliminate( *this, hasher, table() );
    }

    void _por() {
        if ( shared.g.porEliminate( domain(), *this ) )
            shared.need_expand = true;
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
        typedef visitor::Parallel< Setup, This, Hasher > Visitor;

        Visitor visitor( shared.g, *this, *this, hasher, &table() );
        queueAll( visitor );
        visitor.processQueue();
    }

    void elimination() {
        shared.size = 0;
        domain().parallel().run( shared, &This::_elimination );
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
        typedef visitor::Parallel< Setup, This, Hasher > Visitor;

        Visitor visitor( shared.g, *this, *this, hasher, &table() );
        assert( shared.cycle_node.valid() );
        visitor.queue( Blob(), shared.cycle_node );
        visitor.processQueue();
    }

    void _counterexample() {
        for ( int i = 0; i < table().size(); ++i ) {
            if ( cycleFound() ) {
                shared.cycle_node = cycleNode();
                shared.cycle_found = true;
                return;
            }
            Node st = shared.cycle_node = table()[ i ].key;
            if ( !st.valid() )
                continue;
            if ( extension( st ).iteration == shared.iteration )
                continue; // already seen
            if ( !extension( st ).inS || !extension( st ).inF )
                continue;
            domain().parallel().run( shared, &This::_checkCycle );
        }
    }

    void _parentTrace() {
        ce.setup( shared.g, shared ); // XXX this will be done many times needlessly
        ce._parentTrace( *this, hasher, equal, table() );
    }

    void _traceCycle() {
        ce._traceCycle( *this, hasher, table() );
    }

    void counterexample() {
        if ( !cycleFound() ) {
            progress() << " obtaining counterexample...      " << std::flush;
            domain().parallel().runInRing(
                shared, &This::_counterexample );
            progress() << "done" << std::endl;
        }

        shared.ce.initial = cycleNode();
        assert( cycleFound() );

        ce.setup( shared.g, shared );
        ce.lasso( domain(), *this );
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
        for ( int i = 0; i < domain().peers(); ++i ) {
            shared.stats.merge( domain().shared( i ).stats );
        }
        shared.stats.updateResult( result() );
        shared.stats = algorithm::Statistics< G >();
    }

    Result run()
    {
        size_t oldsize = 0;

        result().fullyExplored = Result::Yes;
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

        if ( want_ce && !valid )
            counterexample();

        result().ltlPropertyHolds = valid ? Result::Yes : Result::No;
        return result();
    }

    Owcty( Config *c = 0 )
        : Algorithm( c, sizeof( Extension ) )
    {
        initGraph( shared.g );
        shared.size = 0;
        if ( c ) {
            this->becomeMaster( &shared, workerCount( c ) );
            shared.initialTable = c->initialTableSize();
        }
    }
};

}
}

#endif
