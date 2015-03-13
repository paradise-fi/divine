// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/graph/por.h>

#ifndef DIVINE_ALGORITHM_POR_C3_H
#define DIVINE_ALGORITHM_POR_C3_H

namespace divine {
namespace algorithm {

// Implements a (parallel) check of the POR cycle proviso.
template< typename G, typename Store, typename Statistics >
struct PORGraph : graph::Transform< G > {
    typedef PORGraph< G, Store, Statistics > This;
    typedef typename G::Node Node;
    typedef typename G::Label Label;

    using Vertex = typename Store::Vertex;
    using Handle = typename Store::Handle;

    int m_algslack;

    struct Extension {
        int predCount:29;
        bool full:1;
        bool done:1;
        bool remove:1;
    };

    std::vector< Handle > to_expand;
    bool finished;

    void updatePredCount( Vertex t, int v ) {
        extension( t ).predCount = v;
    }

    PORGraph() {
        this->base().initPOR();
    }

    int setSlack( int s ) {
        m_algslack = s;
        return graph::Transform< G >::setSlack( s + sizeof( Extension ) );
    }

    Extension &extension( Vertex n ) {
        return n.template extension< Extension >( m_algslack );
    }

    template < typename T >
    int predCount( T n ) {
        return extension( n ).predCount;
    }

    bool full( Vertex n ) {
        return extension( n ).full;
    }

    template< typename Alloc, typename Yield >
    void successors( Alloc alloc, Vertex st, Yield yield ) {
        if ( extension( st ).full )
            this->base().successors( alloc, st.node(), yield );
        else
            this->base().ample( alloc, st.node(), yield );
    }

    void porTransition( Store &s, Vertex f, Vertex t ) {

        if ( extension( t ).done )
            return; // ignore

        // increase predecessor count
        if ( s.valid( f ) )
            updatePredCount( t, predCount( t ) + 1 );
    }

    template< typename Setup >
    struct POREliminate : algorithm::Visit< This, Setup >
    {
        static visitor::ExpansionAction expansion( This &, Vertex ) {
            return visitor::ExpansionAction::Expand;
        }

        static visitor::TransitionAction transition( This &t, Vertex, Vertex to, Label ) {
            if ( t.extension( to ).done )
                return visitor::TransitionAction::Forget;

            ASSERT( t.predCount( to ) );
            t.updatePredCount( to, t.predCount( to ) - 1 );
            t.extension( to ).remove = true;
            if ( t.predCount( to ) == 0 ) {
                t.extension( to ).done = true;
                return visitor::TransitionAction::Expand;
            }
            return visitor::TransitionAction::Forget;
        }

        template< typename V >
        static void init( This &t, V &v )
        {
            t.finished = true;

            for ( auto n : v.store() )
            {
                if ( t.extension( n ).done )
                    continue;

                t.finished = false;

                if ( !t.predCount( n ) || t.extension( n ).remove ) {
                    if ( t.predCount( n ) ) {
                        t.to_expand.push_back( n.handle() );
                    }
                    t.updatePredCount( n, 1 ); // ...
                    v.queue( Vertex(), n.node(), Label() ); // coming from "nowhere"
                    n.disown();
                }
            }
        }
    };

    template< typename Algorithm >
    void _porEliminate( Algorithm &a ) {
        typedef POREliminate< typename Algorithm::Setup > Elim;
        auto visitor = a.makeVisitor( *this, a, Elim() );

        Elim::init( *this, visitor );
        visitor.processQueue();
    }

    void breakStalemate( Store &store ) { // this is NOT deterministic :/
        for ( auto n : store ) {
            if ( !extension( n ).done ) {
                updatePredCount( n, 0 );
                to_expand.push_back( n.handle() );
                break;
            }
        }
    }

    // gives true iff there are nodes that need expanding
    template< typename Domain, typename Alg >
    bool porEliminate( Domain &d, Alg &a ) {

        to_expand.clear();

        while ( true ) {
            d.parallel( &Alg::_por_worker ); // calls _porEliminate

            if ( finished )
                break;

            breakStalemate( a.store() );
        }

        return to_expand.size() > 0;
    }

    template< typename Algorithm >
    bool porEliminateLocally( Algorithm &a ) {
        typedef POREliminate< typename Algorithm::Setup > Elim;
        visitor::BFV< Elim > visitor( *this, *this, a.store() );

        to_expand.clear();

        while ( true ) {
            Elim::init( *this, visitor );
            visitor.processQueue();

            if ( finished )
                break;

            breakStalemate( a.store() );
        }

        return to_expand.size() > 0;
    }

    template< typename Alloc, typename Yield >
    void porExpand( Alloc alloc, Store& store, Yield yield )
    {
        for ( auto h : to_expand )
            fullexpand( alloc, yield, store.vertex( h ) );
    }

    /* This closure is used instead of lambda to work-around bug in clang 3.4
     * which causes compiler to terminate with SIGSEGV
     * http://llvm.org/bugs/show_bug.cgi?id=18473
     */
    template< typename Set >
    struct SuccInserter {
        Set &set;

        SuccInserter( Set &set ) : set( set ) { }

        void operator()( Node x, Label l ) { set.insert( std::make_pair( x, l ) ); }
    };

    template< typename Set >
    SuccInserter< Set > succInserter( Set &set ) { return SuccInserter< Set >( set ); }

    template< typename Alloc, typename Yield >
    void fullexpand( Alloc alloc, Yield yield, Vertex v ) {
        extension( v ).full = true;
        BlobComparerLT bcomp( this->pool() );
        std::set< std::pair< Node, Label >, BlobComparerLT > all( bcomp ) , ample( bcomp ), out( bcomp );
        std::vector< std::pair< Node, Label > > extra;

        this->base().successors( alloc, v.node(), succInserter( all ) );
        this->base().ample( alloc, v.node(), succInserter( ample ) );

        std::set_difference( all.begin(), all.end(), ample.begin(), ample.end(),
                             std::inserter( out, out.begin() ), bcomp );

        // accumulate all the extra states we have generated
        std::copy( ample.begin(), ample.end(), std::back_inserter( extra ) );
        std::set_intersection( all.begin(), all.end(), ample.begin(), ample.end(),
                               std::back_inserter( extra ), bcomp );

        // release the states that we aren't going to use
        for ( auto i : extra )
            this->base().release( i.first );

        for ( auto i : out )
            yield( v, i.first, i.second );
    }
};

}
}

#endif
