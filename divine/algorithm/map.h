// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/ltlce.h>

#ifndef DIVINE_MAP_H
#define DIVINE_MAP_H

namespace divine {
namespace algorithm {

template< typename > struct Map;

// ------------------------------------------
// -- Some drudgery for MPI's sake
// --
template< typename G >
struct _MpiId< Map< G > >
{
    static void (Map< G >::*from_id( int n ))()
    {
        switch ( n ) {
            case 0: return &Map< G >::_visit;
            case 1: return &Map< G >::_parentTrace;
            case 2: return &Map< G >::_traceCycle;
            default: assert_die();
        }
    }

    static int to_id( void (Map< G >::*f)() ) {
        if( f == &Map< G >::_visit ) return 0;
        if( f == &Map< G >::_parentTrace ) return 1;
        if( f == &Map< G >::_traceCycle ) return 2;
        assert_die();
    }

    template< typename O >
    static void writeShared( typename Map< G >::Shared s, O o ) {
        *o++ = s.initialTable;
        *o++ = s.iteration;
    }

    template< typename I >
    static I readShared( typename Map< G >::Shared &s, I i ) {
        s.initialTable = *i++;
        s.iteration = *i++;
        return i;
    }
};

/**
 * Implementation of Maximal Accepting Predecessors fair cycle detection
 * algorithm. L. Brim, I. Černá, P. Moravec, and J. Šimša. Accepting
 * Predecessors are a Better than Back Edges in Distributed LTL
 * Model-Checking. In 5th International Conference on Formal Methods in
 * Computer-Aided Design (FM-CAD'04), volume 3312 of LNCS, pages
 * 352–366. Springer-Verlag, 2004.
 */
template< typename G >
struct Map : Algorithm, DomainWorker< Map< G > >
{
    typedef typename G::Node Node;

    struct Shared {
        int expanded, eliminated, accepting;
        int iteration;
        int initialTable;
        G g;
        CeShared< Node > ce;
    } shared;

    struct Extension {
        Blob parent;
        unsigned id:30;
        unsigned elim:2;
        unsigned map:30;
        unsigned oldmap:30;
        unsigned iteration;
    };

    LtlCE< G, Shared, Extension > ce;

    Domain< Map< G > > &domain() {
        return DomainWorker< Map< G > >::domain();
    }

    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    Node cycleNode() {
        for ( int i = 0; i < domain().peers(); ++i ) {
            if ( domain().shared( i ).ce.initial.valid() )
                return domain().shared( i ).ce.initial;
        }
        return Node();
    }

    visitor::TransitionAction updateIteration( Node t ) {
        int old = extension( t ).iteration;
        extension( t ).iteration = shared.iteration;
        return (old != shared.iteration) ?
            visitor::ExpandTransition :
            visitor::ForgetTransition;
    }


    visitor::ExpansionAction expansion( Node st )
    {
        ++ shared.expanded;
        if ( shared.g.isAccepting ( st ) ) {
            if ( !extension( st ).id ) { // first encounter
                extension( st ).id = reinterpret_cast< intptr_t >( st.ptr );
                ++ shared.accepting;
            }
            if ( extension( st ).id == extension( st ).map ) {
                // found accepting cycle
                shared.ce.initial = st;
                return visitor::TerminateOnState;
            }
        }
        return visitor::ExpandState;
    }

    std::pair< int, bool > map( Node f, Node st )
    {
        int map;
        bool elim = false;
        if ( extension( f ).oldmap != 0 && extension( st ).oldmap != 0 )
            if ( extension( f ).oldmap != extension( st ).oldmap )
                return std::make_pair( -1, false );

        if ( extension( f ).map < extension( f ).id ) {
            if ( extension( f ).elim == 2 )
                map = extension( f ).map;
            else {
                map = extension( f ).id;
                if ( extension( f ).elim != 1 )
                    elim = true;
            }
        } else
            map = extension( f ).map;

        return std::make_pair( map, elim );
    }

    visitor::TransitionAction transition( Node f, Node t )
    {
        if ( !f.valid() ) {
            assert( equal( t, shared.g.initial() ) );
            return updateIteration( t );
        }

        if ( extension( f ).id == 0 )
            assert( !shared.g.isAccepting( f ) );
        if ( shared.g.isAccepting( f ) )
            assert( extension( f ).id > 0 );

        if ( !extension( t ).parent.valid() )
            extension( t ).parent = f;

        std::pair< int, bool > a = map( f, t );
        int map = a.first;
        bool elim = a.second;

        if ( map == -1 )
            return updateIteration( t );

        if ( extension( t ).map < map ) {
            if ( elim ) {
                ++ shared.eliminated;
                extension( f ).elim = 1;
            }
            extension( t ).map = map;
            return visitor::ExpandTransition;
        }

        return updateIteration( t );
    }

    void _visit() {
        typedef visitor::Setup< G, Map< G >, Table > Setup;
        typedef visitor::Parallel< Setup, Map< G >, Hasher > Visitor;

        shared.expanded = 0;
        shared.eliminated = 0;
        m_initialTable = &shared.initialTable;

        Visitor visitor( shared.g, *this, *this, hasher, &table() );
        if ( visitor.owner( shared.g.initial() ) == this->globalId() )
            visitor.queue( Blob(), shared.g.initial() );
        visitor.processQueue();

        for ( size_t i = 0; i < table().size(); ++i ) {
            Node st = table()[ i ].key;
            if ( st.valid() ) {
                extension( st ).oldmap = extension( st ).map;
                extension( st ).map = 0;
                if ( extension( st ).elim == 1 )
                    extension( st ).elim = 2;
            }
        }
    }

    void visit() {
        domain().parallel().run( shared, &Map< G >::_visit );
    }

    void _parentTrace() {
        ce.setup( shared.g, shared );
        ce._parentTrace( *this, hasher, equal, table() );
    }

    void _traceCycle() {
        ce._traceCycle( *this, hasher, table() );
    }

    Result run()
    {
        shared.iteration = 1;
        int acceptingCount = 0, eliminated = 0, d_eliminated = 0, expanded = 0;
        bool valid = true;
        do {
            std::cerr << " iteration " << std::setw( 3 ) << shared.iteration
                      << "...\t" << std::flush;
            shared.accepting = shared.eliminated = shared.expanded = 0;
            visit();
            d_eliminated = 0;
            expanded = 0;
            for ( int i = 0; i < domain().peers(); ++ i ) {
                if ( shared.iteration == 1 )
                    acceptingCount += domain().shared( i ).accepting;
                d_eliminated += domain().shared( i ).eliminated;
                expanded += domain().shared( i ).expanded;
            }
            eliminated += d_eliminated;
            assert_leq( eliminated, acceptingCount );
            std::cerr << eliminated << " eliminated, "
                      << expanded << " expanded" << std::endl;
            ++ shared.iteration;
            valid = !cycleNode().valid();
        } while ( d_eliminated > 0 && valid );

        result().ltlPropertyHolds = valid ? Result::Yes : Result::No;

        livenessBanner( valid );

        if ( !valid && want_ce ) {
            std::cerr << " generating counterexample..." << std::endl;
            assert( cycleNode().valid() );
            shared.ce.initial = cycleNode();
            ce.setup( shared.g, shared );
            ce.lasso( domain(), *this );
        }

        return result();
    }

    Map( Config *c = 0 )
        : Algorithm( c, sizeof( Extension ) )
    {
        initGraph( shared.g );
        if ( c ) {
            becomeMaster( &shared, workerCount( c ) );
            shared.initialTable = c->initialTableSize();
        }
    }

};

}
}

#endif
