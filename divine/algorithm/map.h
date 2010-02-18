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

    struct NodeId {
        uintptr_t ptr;
        short owner;

        bool operator<( const NodeId other ) const {
            if ( owner < other.owner )
                return true;
            if ( owner > other.owner )
                return false;
            return ptr < other.ptr;
        }

        bool operator!=( const NodeId other ) const {
            return ptr != other.ptr || owner != other.owner;
        }

        bool operator==( const NodeId other ) const {
            return ptr == other.ptr && owner == other.owner;
        }

        bool valid() const {
            return ptr != 0;
        }

        friend std::ostream &operator<<( std::ostream &o, NodeId i ) {
            return o << "(" << i.owner << ", " << i.ptr << "[" << i.ptr % 1024 << "])";
        }
    } __attribute__((packed));

    NodeId makeId( Node n ) {
        NodeId ret;
        ret.ptr = reinterpret_cast< intptr_t >( n.data() );
        ret.owner = this->globalId();
        return ret;
    }

    struct Extension {
        Blob parent;
        bool seen:1;
        short iteration:14;
        // elim: 0 = candidate for elimination, 1 = not a canditate, 2 = eliminated
        short elim:2;
        NodeId map;
        NodeId oldmap;
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

    bool isAccepting( Node st ) {
        if ( !shared.g.isAccepting ( st ) )
            return false;
        if ( extension( st ).elim == 2 )
            return false;
        return true;
    }

    visitor::ExpansionAction expansion( Node st )
    {
        ++ shared.expanded;
        if ( shared.g.isAccepting ( st ) && !extension( st ).seen ) {
            extension( st ).seen = true;
            ++ shared.accepting;
        }
        return visitor::ExpandState;
    }

    visitor::TransitionAction transition( Node f, Node t )
    {
        if ( !f.valid() ) {
            assert( equal( t, shared.g.initial() ) );
            return updateIteration( t );
        }

        if ( !extension( t ).parent.valid() )
            extension( t ).parent = f;

        if ( isAccepting( t ) && extension( f ).map == makeId( t ) ) {
            shared.ce.initial = t;
            return visitor::TerminateOnTransition;
        }

        if ( extension( f ).oldmap.valid() && extension( t ).oldmap.valid() )
            if ( extension( f ).oldmap != extension( t ).oldmap )
                return updateIteration( t );

        if ( isAccepting( t ) )
            extension( t ).map = std::max( extension( t ).map, makeId( t ) );
        NodeId map = std::max( extension( f ).map, extension( t ).map );

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
                extension( st ).map = NodeId();
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
        } while ( d_eliminated > 0 && eliminated < acceptingCount && valid );

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
