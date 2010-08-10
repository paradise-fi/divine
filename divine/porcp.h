// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <wibble/list.h>

#ifndef DIVINE_PORCP_H
#define DIVINE_PORCP_H

namespace divine {
namespace algorithm {

namespace list = wibble::list;

template< typename G >
struct NonPORGraph : generator::Extended< G > {
    bool eliminate_done;

    NonPORGraph() : eliminate_done( false ) {}

    void porExpand( typename G::Node ) {}

    template< typename Domain, typename Alg >
    bool porEliminate( Domain &, Alg & ) {
        eliminate_done = true;
        return false;
    }

    template< typename Q >
    void queueInitials( Q &q ) {
        if ( eliminate_done )
            return;
        this->g().queueInitials( q );
    }
};

// Implements a (parallel) check of the POR cycle proviso.
template< typename G, typename Statistics >
struct PORGraph : generator::Extended< G > {
    typedef typename G::Node Node;
    typedef typename G::Successors Successors;

    int m_algslack;

    struct Extension {
        int predCount:29;
        bool full:1;
        bool done:1;
        bool remove:1;
    };

    int setSlack( int s ) {
        m_algslack = s;
        return generator::Extended< G >::setSlack( s + sizeof( Extension ) );
    }

    Extension &extension( Node n ) {
        return n.template get< Extension >( m_algslack );
    }

    Successors successors( Node st ) {
        if ( extension( st ).full )
            return this->g().successors( st );
        else
            return this->g().ample( st );
    }

    std::set< Node > to_check, to_expand;

    void porExpand( Node n ) {
        to_check.insert( n );
    }

    template< typename PC >
    void porTransition( Node f, Node t, int *predcount ) {
        if ( extension( t ).done )
            return; // ignore

        /* std::cerr << "transition ("
                  << extension( t ).predCount << "): "
                  << showNode( f ) << " -> " << showNode( t ) << std::endl; */

        // increase predecessor count
        if ( f.valid() ) {
            if ( predcount )
                ++ *predcount;
            ++ extension( t ).predCount;
        }
    }

    visitor::ExpansionAction elimExpansion( Node n ) {
        // std::cerr << "elimExpansion " << showNode( n ) << std::endl;
        return visitor::ExpandState;
    }

    visitor::TransitionAction elimTransition( Node from, Node to ) {
        if ( extension( to ).done )
            return visitor::ForgetTransition;

        /* std::cerr << "elimTransition ("
                  << extension( to ).predCount << "): "
                  << showNode( from ) << " -> " << showNode( to ) << std::endl; */

        assert( extension( to ).predCount );
        -- extension( to ).predCount;
        extension( to ).remove = true;
        if ( extension( to ).predCount == 0 ) {
            extension( to ).done = true;
            return visitor::ExpandTransition;
        }
        return visitor::ForgetTransition;
    }

    template< typename Worker, typename Hasher, typename Table >
    void _eliminate( Worker &w, Hasher &h, Table &t ) {
        typedef PORGraph< G, Extension > Us;
        typedef visitor::Setup< Us, Us, Table, Statistics,
            &Us::elimTransition,
            &Us::elimExpansion > Setup;
        typedef visitor::Parallel< Setup, Worker, Hasher > Visitor;

        Visitor visitor( *this, w, *this, h, &t );
        for ( typename std::set< Node >::iterator j, i = to_check.begin(); i != to_check.end(); i = j ) {
            j = i; ++j;
            if ( !extension( *i ).predCount || extension( *i ).remove ) {
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
            if ( extension ( *i ).done )
                to_check.erase( i );
        }
    }

    // gives true iff there are nodes that need expanding
    template< typename Domain, typename Alg >
    bool porEliminate( Domain &d, Alg &a ) {
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
    void queueInitials( Visitor &v ) {
        if ( to_expand.size() == 0 )
            this->g().queueInitials( v );

        for ( typename std::set< Node >::iterator i = to_expand.begin();
              i != to_expand.end(); ++i ) {
            fullexpand( v, *i );
        }
        to_expand.clear();
    }

    template< typename Visitor >
    void fullexpand( Visitor &v, Node n ) {
        extension( n ).full = true;
        std::set< Node > all, ample, out;
        // leak!
        list::output( this->g().successors( n ), std::inserter( all, all.begin() ) );
        list::output( this->g().ample( n ), std::inserter( ample, ample.begin() ) );
        std::set_difference( all.begin(), all.end(), ample.begin(), ample.end(),
                             std::inserter( out, out.begin() ) );
        for ( typename std::set< Node >::iterator i = out.begin(); i != out.end(); ++i ) {
            const_cast< Blob* >( &*i )->header().permanent = 1;
            v.queue( n, *i );
        }
    }

    template< typename O >
    void fullexpand_stl( O o, Node n ) {
        extension( n ).full = true;
        std::set< Node > all, ample, out;
        // leak!
        list::output( this->g().successors( n ), std::inserter( all, all.begin() ) );
        list::output( this->g().ample( n ), std::inserter( ample, ample.begin() ) );
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
