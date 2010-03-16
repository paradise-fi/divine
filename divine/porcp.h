// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <wibble/list.h>

#ifndef DIVINE_PORCP_H
#define DIVINE_PORCP_H

namespace divine {
namespace algorithm {

namespace list = wibble::list;

struct PorExt {
    bool full:1;
    bool done:1;
    bool remove:1;
};

// Implements a (parallel) check of the POR cycle proviso.
template< typename G, typename Extension >
struct PorGraph : generator::Common {
    G *_g;
    typedef typename G::Node Node;
    typedef typename G::Successors Successors;

    G &g() { assert( _g ); return *_g; }
    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    Node initial() { return g().initial(); }
    Successors successors( Node st ) {
        if ( extension( st ).por.full )
            return g().successors( st );
        else
            return g().ample( st );
    }

    void release( Node s ) { g().release( s ); }
    bool isDeadlock( Node s ) { return g().isDeadlock( s ); }
    bool isGoal( Node s ) { return g().isGoal( s ); }
    bool isAccepting( Node s ) { return g().isAccepting( s ); }
    std::string showNode( Node s ) { return g().showNode( s ); }
    void read( std::string path ) { g().read( path ); }

    PorGraph() : _g( 0 ) {}
    void setG( G &g ) { _g = &g; }
    PorGraph &operator=( const PorGraph &other ) { return *this; }
    PorGraph( const PorGraph & ) : _g( 0 ) {}

    std::set< Node > to_check, to_expand;

    void expansion( Node n ) {
        to_check.insert( n );
    }

    void transition( Node f, Node t ) {
        if ( extension( t ).por.done )
            return; // ignore

        /* std::cerr << "transition ("
                  << extension( t ).predCount << "): "
                  << showNode( f ) << " -> " << showNode( t ) << std::endl; */

        // increase predecessor count
        if ( f.valid() )
            ++ extension( t ).predCount;
    }

    visitor::ExpansionAction elimExpansion( Node n ) {
        // std::cerr << "elimExpansion " << showNode( n ) << std::endl;
        return visitor::ExpandState;
    }

    visitor::TransitionAction elimTransition( Node from, Node to ) {
        if ( extension( to ).por.done )
            return visitor::ForgetTransition;

        /* std::cerr << "elimTransition ("
                  << extension( to ).predCount << "): "
                  << showNode( from ) << " -> " << showNode( to ) << std::endl; */

        assert( extension( to ).predCount );
        -- extension( to ).predCount;
        extension( to ).por.remove = true;
        if ( extension( to ).predCount == 0 ) {
            extension( to ).por.done = true;
            return visitor::ExpandTransition;
        }
        return visitor::ForgetTransition;
    }

    template< typename Worker, typename Hasher, typename Table >
    void _eliminate( Worker &w, Hasher &h, Table &t ) {
        typedef PorGraph< G, Extension > Us;
        typedef visitor::Setup< Us, Us, Table,
            &Us::elimTransition,
            &Us::elimExpansion > Setup;
        typedef visitor::Parallel< Setup, Worker, Hasher > Visitor;

        Visitor visitor( *this, w, *this, h, &t );
        for ( typename std::set< Node >::iterator j, i = to_check.begin(); i != to_check.end(); i = j ) {
            j = i; ++j;
            if ( !extension( *i ).predCount || extension( *i ).por.remove ) {
                if ( extension( *i ).predCount ) {
                    // std::cerr << "to expand: " << showNode( *i ) << std::endl;
                    to_expand.insert( *i );
                }
                extension( *i ).predCount = 1; // ...
                visitor.queue( Blob(), *i );
                to_check.erase( i );
            }
        }
        visitor.processQueue();
        for ( typename std::set< Node >::iterator j, i = to_check.begin();
              i != to_check.end(); i = j ) {
            j = i; ++j;
            if ( extension ( *i ).por.done )
                to_check.erase( i );
        }
    }

    // gives true iff there are nodes that need expanding
    template< typename Domain, typename Alg >
    bool eliminate( Domain &d, Alg &a ) {
        int checked = to_check.size();

        while ( !to_check.empty() ) {
            d.parallel().run( a.shared, &Alg::_eliminate_worker );
            /* std::cerr << "eliminate: " << to_check.size() << " nodes remaining, "
               << to_expand.size() << " to expand" << std::endl; */

            if ( to_check.empty() )
                break;

            // std::cerr << "breaking stalemate at: " << showNode( *to_check.begin() ) << std::endl;
            extension( *to_check.begin() ).predCount = 0;
            to_expand.insert( *to_check.begin() );
        }
        /* std::cerr << "eliminate: " << checked << " checked, "
           << to_expand.size() << " to expand" << std::endl; */

        return to_expand.size() > 0;
    }

    template< typename Visitor >
    void queue( Visitor &v ) {
        for ( typename std::set< Node >::iterator i = to_expand.begin();
              i != to_expand.end(); ++i ) {
            fullexpand( v, *i );
        }
        to_expand.clear();
    }

    template< typename Visitor >
    void fullexpand( Visitor &v, Node n ) {
        extension( n ).por.full = true;
        std::set< Node > all, ample, out;
        // leak!
        list::output( g().successors( n ), std::inserter( all, all.begin() ) );
        list::output( g().ample( n ), std::inserter( ample, ample.begin() ) );
        std::set_difference( all.begin(), all.end(), ample.begin(), ample.end(),
                             std::inserter( out, out.begin() ) );
        for ( typename std::set< Node >::iterator i = out.begin(); i != out.end(); ++i ) {
            const_cast< Blob* >( &*i )->header().permanent = 1;
            v.queue( n, *i );
        }
    }

    template< typename O >
    void fullexpand_stl( O o, Node n ) {
        extension( n ).por.full = true;
        std::set< Node > all, ample, out;
        // leak!
        list::output( g().successors( n ), std::inserter( all, all.begin() ) );
        list::output( g().ample( n ), std::inserter( ample, ample.begin() ) );
        std::set_difference( all.begin(), all.end(), ample.begin(), ample.end(),
                             std::inserter( out, out.begin() ) );
        for ( typename std::set< Node >::iterator i = out.begin(); i != out.end(); ++i ) {
            const_cast< Blob* >( &*i )->header().permanent = 1;
            *o++ = *i;
        }
    }
};

}
}

#endif
