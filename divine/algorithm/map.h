// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/ltlce.h>

#ifndef DIVINE_MAP_H
#define DIVINE_MAP_H

namespace divine {
namespace algorithm {

template< typename, typename > struct Map;

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

// ------------------------------------------
// -- Some drudgery for MPI's sake
// --
template< typename G, typename S >
struct _MpiId< Map< G, S > >
{
    typedef Map< G, S > A;

    static void (Map< G, S >::*from_id( int n ))()
    {
        switch ( n ) {
            case 0: return &A::_visit;
            case 1: return &A::_cleanup;
            case 2: return &A::_parentTrace;
            case 3: return &A::_traceCycle;
            case 7: return &A::_por;
            case 8: return &A::_por_worker;
            default: assert_die();
        }
    }

    static int to_id( void (A::*f)() ) {
        if( f == &A::_visit ) return 0;
        if( f == &A::_cleanup ) return 1;
        if( f == &A::_parentTrace ) return 2;
        if( f == &A::_traceCycle ) return 3;
        if( f == &A::_por) return 7;
        if( f == &A::_por_worker) return 8;
        assert_die();
    }

    template< typename O >
    static void writeShared( typename A::Shared s, O o ) {
        o = s.stats.write( o );
        *o++ = s.need_expand;
        *o++ = s.initialTable;
        *o++ = s.iteration;
        *o++ = s.accepting;
        *o++ = s.expanded;
        *o++ = s.eliminated;
        o = s.ce.write( o );
    }

    template< typename I >
    static I readShared( typename A::Shared &s, I i ) {
        i = s.stats.read( i );
        s.need_expand = *i++;
        s.initialTable = *i++;
        s.iteration = *i++;
        s.accepting = *i++;
        s.expanded = *i++;
        s.eliminated = *i++;
        i = s.ce.read( i );
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
template< typename G, typename _Statistics >
struct Map : Algorithm, DomainWorker< Map< G, _Statistics > >
{
    typedef typename G::Node Node;
    typedef Map< G, _Statistics > This;

    struct Shared {
        int expanded, eliminated, accepting;
        int iteration;
        int initialTable;
        G g;
        CeShared< Node > ce;
        algorithm::Statistics< G > stats;
        bool need_expand;
    } shared;

    int d_eliminated,
        acceptingCount,
        eliminated,
        expanded;
    Node cycle_node;

    VertexId makeId( Node n ) {
        VertexId ret;
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
        VertexId map;
        VertexId oldmap;
    };

    LtlCE< G, Shared, Extension > ce;

    Domain< This > &domain() {
        return DomainWorker< This >::domain();
    }

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
        for ( int i = 0; i < domain().peers(); ++i )
            shared.stats.merge( domain().shared( i ).stats );
        shared.stats.updateResult( result() );
        shared.stats = algorithm::Statistics< G >();
        for ( int i = 0; i < domain().peers(); ++ i ) {
            if ( shared.iteration == 1 )
                acceptingCount += domain().shared( i ).accepting;
            d_eliminated += domain().shared( i ).eliminated;
            expanded += domain().shared( i ).expanded;
            assert_eq( shared.eliminated, 0 );
            assert_eq( shared.expanded, 0 );

            if ( domain().shared( i ).ce.initial.valid() )
                cycle_node = domain().shared( i ).ce.initial;
        }
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
        if ( !extension( st ).seen ) {
            extension( st ).seen = true;
            if ( shared.g.isAccepting ( st ) )
                ++ shared.accepting;
            shared.g.porExpansion( st );
            shared.stats.addNode( shared.g, st );
        } else
            shared.stats.addExpansion();

        return visitor::ExpandState;
    }

    visitor::TransitionAction transition( Node f, Node t )
    {
        if ( shared.iteration == 1 ) {
            shared.g.porTransition( f, t, 0 );
            shared.stats.addEdge();
        }

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
        VertexId map = std::max( extension( f ).map, extension( t ).map );

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
        typedef visitor::Parallel< Setup, This, Hasher > Visitor;

        shared.expanded = 0;
        shared.eliminated = 0;
        m_initialTable = &shared.initialTable;

        Visitor visitor( shared.g, *this, *this, hasher, &table() );
        shared.g.queueInitials( visitor );
        visitor.processQueue();
    }

    void _cleanup() {
        for ( size_t i = 0; i < table().size(); ++i ) {
            Node st = table()[ i ].key;
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
        while ( !this->idle() );
    }

    void _por_worker() {
        shared.g._porEliminate( *this, hasher, table() );
    }

    void _por() {
        if ( shared.g.porEliminate( domain(), *this ) )
            shared.need_expand = true;
    }

    void visit() {
        domain().parallel().run( shared, &This::_visit );
        collect();

        while ( !cycle_node.valid() && shared.iteration == 1 ) {
            shared.need_expand = false;
            domain().parallel().runInRing( shared, &This::_por );

            if ( shared.need_expand ) {
                domain().parallel().run( shared, &This::_visit );
                collect();
            } else
                break;
        }

        domain().parallel().run( shared, &This::_cleanup );
        collect();
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
        acceptingCount = eliminated = d_eliminated = expanded = 0;
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

            ++ shared.iteration;
        } while ( d_eliminated > 0 && eliminated < acceptingCount && valid );

        result().ltlPropertyHolds = valid ? Result::Yes : Result::No;

        livenessBanner( valid );

        if ( !valid && want_ce ) {
            progress() << " generating counterexample...     " << std::flush;
            assert( cycle_node.valid() );
            shared.ce.initial = cycle_node;
            ce.setup( shared.g, shared );
            ce.lasso( domain(), *this );
            progress() << "done" << std::endl;
        }

        return result();
    }

    Map( Config *c = 0 )
        : Algorithm( c, sizeof( Extension ) )
    {
        initGraph( shared.g );
        if ( c ) {
            this->becomeMaster( &shared, workerCount( c ) );
            shared.initialTable = c->initialTableSize();
        }
    }

};

}
}

#endif
