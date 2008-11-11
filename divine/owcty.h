// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/visitor.h>
#include <divine/parallel.h>
#include <divine/report.h>

#ifndef DIVINE_OWCTY_H
#define DIVINE_OWCTY_H

namespace divine {
namespace algorithm {

template< typename G >
struct Owcty : Domain< Owcty< G > >
{
    typedef typename G::Node Node;

    struct Shared {
        size_t size;
        Node cycle;
        int iteration;
        G g;
    } shared;

    struct Extension {
        Blob parent, cycle; // TODO We only want to keep one of those two.
        uintptr_t map;
        size_t predCount:15;
        size_t iteration:15;
        bool inS:1;
        bool inF:1;
    };

    struct Hasher {
        int size;
        Hasher( int s = 0 ) : size( s ) {}
        inline hash_t operator()( Blob b ) {
            return b.hash( 0, size );
        }
    };

    struct Equal {
        int size;
        Equal( int s = 0 ) : size( s ) {}
        inline hash_t operator()( Blob a, Blob b ) {
            return a.compare( b, 0, size ) == 0;
        }
    };

    typedef HashMap< Node, Unit, Hasher,
                     divine::valid< Node >, Equal > Table;
    Table *m_table;

    Node m_cycleCandidate;
    Node m_mapCycleState;
    bool m_cycleFound;

    // -- generally useful utilities ------------------------

    Table &table() {
        if ( !m_table ) {
            int size = shared.g.stateSize();
            assert( size );
            m_table = new Table( Hasher( size ), divine::valid< Node >(),
                                 Equal( size ) );
        }
        return *m_table;
    }

    Extension &extension( Node n ) {
        int stateSize = shared.g.stateSize();
        assert( stateSize );
        return n.template get< Extension >( stateSize );
    }

    int totalSize() {
        int sz = 0;
        for ( int i = 0; i < this->parallel().n; ++i ) {
            Shared &s = this->parallel().shared( i );
            sz += s.size;
        }
        return sz;
    }

    void resetSize() {
        shared.size = 0;
        for ( int i = 0; i < this->parallel().n; ++i ) {
            Shared &s = this->parallel().shared( i );
            s.size = 0;
        }
    }

    template< typename V >
    void queueAll( V &v ) {
        for ( size_t i = 0; i < table().size(); ++i ) {
            Node st = table()[ i ].key;
            if ( st.valid() && extension( st ).inS && extension( st ).inF )
                v.queue( Blob(), st );
        }
    }

    uintptr_t mapId( Node t ) {
        return reinterpret_cast< uintptr_t >( t.ptr );
    }

    bool updateIteration( Node t, int n ) {
        int old = extension( t ).iteration;
        extension( t ).iteration = 2 * shared.iteration + n;
        return old != 2 * shared.iteration + n;
    }

    // -- reachability pass implementation -----------------

    visitor::ExpansionAction reachExpansion( Node st )
    {
        assert( extension( st ).predCount > 0 );
        assert( extension( st ).inS );
        ++ shared.size;
        return visitor::ExpandState;
    }

    visitor::TransitionAction reachTransition( Node f, Node t )
    {
        ++ extension( t ).predCount;
        extension( t ).inS = true;
        if ( extension( t ).inF && f.valid() )
            return visitor::IgnoreTransition;
        else {
            return updateIteration( t, 0 ) ?
                visitor::ExpandTransition :
                visitor::IgnoreTransition;
        }
    }

    void _reachability() { // parallel
        typedef visitor::Setup< G, Owcty< G >, Table,
            &Owcty< G >::reachTransition,
            &Owcty< G >::reachExpansion > Setup;
        typedef visitor::Parallel< Setup, Owcty< G > > Visitor;
        Visitor visitor( shared.g, *this, *this, &table() );

        queueAll( visitor );
        visitor.visit();
    }

    void reachability() {
        shared.size = 0;
        this->parallel().run( &Owcty< G >::_reachability );
        shared.size = totalSize();
    }

    // -- initialise pass implementation ------------------

    visitor::TransitionAction initTransition( Node from, Node to )
    {
        if ( !extension( to ).parent.valid() )
            extension( to ).parent = from;
        if ( from.valid() ) {
            uintptr_t fromMap = extension( from ).map;
            uintptr_t fromId = mapId( from );
            if ( fromMap > extension( to ).map )
                extension( to ).map = fromMap;
            if ( shared.g.is_accepting( from ) )
                if ( fromId > extension( to ).map )
                    extension( to ).map = fromId;
            if ( mapId( to ) == fromMap ) {
                shared.cycle = to;
                return visitor::TerminateOnTransition;
            }
        }
        return visitor::FollowTransition;
    }

    visitor::ExpansionAction initExpansion( Node st )
    {
        extension( st ).predCount = 0;
        extension( st ).inF = extension( st ).inS = shared.g.is_accepting( st );
        shared.size += extension( st ).inS;
        return visitor::ExpandState;
    }

    void _initialise() { // parallel
        typedef visitor::Setup< G, Owcty< G >, Table,
            &Owcty< G >::initTransition,
            &Owcty< G >::initExpansion > Setup;
        typedef visitor::Parallel< Setup, Owcty< G > > Visitor;
        Visitor visitor( shared.g, *this, *this, &table() );
        shared.g.setAllocator( new BlobAllocator( sizeof( Extension ) ) );

        visitor.visit( shared.g.initial() );
    }
    
    void initialise() {
        this->parallel().run( &Owcty< G >::_initialise );
        shared.size = totalSize();
    }

    // -- elimination pass implementation ------------------

    visitor::ExpansionAction elimExpansion( Node st )
    {
        assert( extension( st ).predCount == 0 );
        extension( st ).inS = false;
        shared.size ++;
        return visitor::ExpandState;
    }

    visitor::TransitionAction elimTransition( Node f, Node t )
    {
        // assert( m_controller.owner( t ) == m_controller.id() );
        assert( t.valid() );
        // assert( !visitor().seen( t ) );
        assert( extension( t ).inS );
        assert( extension( t ).predCount >= 1 );
        -- extension( t ).predCount;
        // we follow a transition only if the target state is going to
        // be eliminated
        if ( extension( t ).predCount == 0 ) {
            return updateIteration( t, 1 ) ?
                visitor::ExpandTransition :
                visitor::IgnoreTransition;
        } else
            return visitor::IgnoreTransition;
    }

    void _elimination() {
        typedef visitor::Setup< G, Owcty< G >, Table,
            &Owcty< G >::elimTransition,
            &Owcty< G >::elimExpansion > Setup;
        typedef visitor::Parallel< Setup, Owcty< G > > Visitor;

        Visitor visitor( shared.g, *this, *this, &table() );
        queueAll( visitor );
        visitor.visit();
    }

    void elimination() {
        int origSize = totalSize();
        shared.size = 0;
        this->parallel().run( &Owcty< G >::_elimination );
        shared.size = origSize - totalSize();
    }


    // -- ... ---

    /* template< typename Bundle, typename Self >
    struct FindCycleImpl :
        ImplCommon,
        observer::Common< Bundle, FindCycleImpl< Bundle, Self > >
    {
        typedef typename Bundle::State State;

        typename Bundle::Controller &m_controller;
        typename Bundle::Visitor &visitor() { return m_controller.visitor(); }

        State transition( State from, State to )
        {
            if ( !to.extension().cycle.valid() ) {
                to.extension().cycle = from;
            }
            if ( from.valid() && to == this->owcty().m_cycleCandidate ) {
                // found cycle
                to.extension().cycle = from;
                this->owcty().m_cycleFound = true;
                m_controller.pack().terminate();
                return ignore( to );
            }
            return follow( to );
        }

        FindCycleImpl( typename Bundle::Controller &c )
            : m_controller( c )
        {
        }
    };

    void mapCycleFound( State st ) {
        m_mapCycleState = st;
    }

    State findCounterexampleFrom( State st ) {
        m_cycleCandidate = st;
        find.blockingVisit( st );
        if ( m_cycleFound )
            return st;
        return State();
    }

    template< typename T >
    State iterateCandidates( T &worker )
    {
        typedef typename T::Visitor::Storage Storage;
        Storage &s = worker.visitor().storage();
        for ( int j = 0; j < s.table().size(); ++j ) {
            State st = s.table()[ j ].key;
            if ( !st.valid() )
                continue;
            if ( worker.visitor().seen( st ) )
                continue;
            if ( !st.extension().inS || !st.extension().inF )
                continue;
            if ( ( st = findCounterexampleFrom( st ) ).valid() )
                return st;
        }
        return State();
    }

    State counterexample() {
        State st;

        m_cycleFound = false;

        for ( int i = 0; i < find.workerCount(); ++i ) {
            swap( reach.worker( i ), find.worker( i ) );
            zeroFlags( find.worker( i ).visitor().storage(), false );
        }

        for ( int i = 0; i < find.workerCount(); ++i ) {
            if ( !st.valid() )
                st = iterateCandidates( find.worker( i ) );
        }

        assert( st.valid() );
        assert( st.extension().inF );

        return st;
        } */

    void printSize() {
        if ( m_mapCycleState.valid() )
            std::cerr << "   (MAP: cycle found)" << std::endl << std::flush;
        else
            std::cerr << "   |S| = " << shared.size << std::endl << std::flush;
    }

    void printIteration( int i ) {
        std::cerr << "------------- iteration " << i
                  << " ------------- " << std::endl;
    }

    // FIXME this should be shared again, later
    void resultBanner( bool valid ) {
        std::cerr << " ===================================== " << std::endl
                  << ( valid ?
                     "       Accepting cycle NOT found       " :
                     "         Accepting cycle FOUND         " )
                  << std::endl
                  << " ===================================== " << std::endl;
    }

    Result run()
    {
        size_t oldsize = 0;

        std::cerr << " initialize...\t\t" << std::flush;
        shared.size = 0;
        initialise();
        printSize();

        shared.iteration = 1;

        if ( !m_mapCycleState.valid() ) {
            do {
                oldsize = shared.size;
                
                printIteration( shared.iteration );
                
                std::cerr << " reachability...\t" << std::flush;
                reachability();
                printSize();
                
                std::cerr << " elimination & reset...\t" << std::flush;
                elimination();
                printSize();
                
                ++shared.iteration;
                
            } while ( oldsize != shared.size && shared.size != 0 );
        }

        bool valid = shared.cycle.valid() ? false : ( shared.size == 0 );
        resultBanner( valid );

        // counterexample generation

        /* if ( !valid && this->config().generateCounterexample() ) {
            std::cerr << " generating counterexample...\t" << std::flush;
            State st;
            if ( shared.cycle.valid() )
                st = findCounterexampleFrom( shared.cycle );
            else
                st = counterexample();
            std::cerr << "   done" << std::endl;

            printCounterexample( init, st );
        } */

        Result res;
        res.ltlPropertyHolds = valid ? Result::Yes : Result::No;
        res.fullyExplored = shared.cycle.valid() ? Result::No : Result::Yes;
        return res;
    }

    Owcty( Config *c = 0 )
        : m_table( 0 )
    {
        shared.size = 0;
        if ( c )
            shared.g.read( c->input() );
    }

};

}
}

#endif
