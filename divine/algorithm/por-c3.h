// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/graph/por.h>

#ifndef DIVINE_ALGORITHM_POR_C3_H
#define DIVINE_ALGORITHM_POR_C3_H

namespace divine {
namespace algorithm {

// Implements a (parallel) check of the POR cycle proviso.
template< typename G, typename Statistics >
struct PORGraph : graph::Transform< G > {
    typedef PORGraph< G, Statistics > This;
    typedef typename G::Node Node;
    typedef typename G::Label Label;

    int m_algslack;

    struct Extension {
        int predCount:29;
        bool full:1;
        bool done:1;
        bool remove:1;
    };

    typedef void (*PredCount)( Node, int );
    PredCount _predCount;

    void updatePredCount( Node t, int v ) {
        extension( t ).predCount = v;
        if ( _predCount )
            _predCount( t, v );
    }

    PORGraph() : _predCount( 0 ) {
        this->base().initPOR();
    }

    int setSlack( int s ) {
        m_algslack = s;
        return graph::Transform< G >::setSlack( s + sizeof( Extension ) );
    }

    Extension &extension( Node n ) {
        return n.template get< Extension >( m_algslack );
    }

    int predCount( Node n ) {
        return extension( n ).predCount;
    }

    bool full( Node n ) {
        return extension( n ).full;
    }

    template< typename Yield >
    void successors( Node st, Yield yield ) {
        if ( extension( st ).full )
            this->base().successors( st, yield );
        else
            this->base().ample( st, yield );
    }

    std::set< Node > to_check, to_expand;

    void porExpansion( Node n ) {
        to_check.insert( n );
    }

    void porTransition( Node f, Node t, PredCount _pc ) {
        _predCount = _pc;

        if ( extension( t ).done )
            return; // ignore

        // increase predecessor count
        if ( f.valid() )
            updatePredCount( t, predCount( t ) + 1 );
    }

    template< typename Setup >
    struct POREliminate : algorithm::Visit< This, Setup >
    {
        static visitor::ExpansionAction expansion( This &, Node n ) {
            return visitor::ExpandState;
        }

        static visitor::TransitionAction transition( This &t, Node from, Node to, Label ) {
            if ( t.extension( to ).done )
                return visitor::ForgetTransition;

            assert( t.predCount( to ) );
            t.updatePredCount( to, t.predCount( to ) - 1 );
            t.extension( to ).remove = true;
            if ( t.predCount( to ) == 0 ) {
                t.extension( to ).done = true;
                return visitor::ExpandTransition;
            }
            return visitor::ForgetTransition;
        }

        template< typename Q >
        static void init( This &t, Q &q )
        {
            for ( typename std::set< Node >::iterator j, i = t.to_check.begin();
                  i != t.to_check.end(); i = j )
            {
                j = i; ++j;
                if ( !t.predCount( *i ) || t.extension( *i ).remove ) {
                    if ( t.predCount( *i ) ) {
                        t.to_expand.insert( *i );
                    }
                    t.updatePredCount( *i, 1 ); // ...
                    q.queue( Blob(), *i, Label() ); // coming from "nowhere"
                    t.to_check.erase( i );
                }
            }
        }

        static void cleanup( This &t )
        {
            for ( typename std::set< Node >::iterator j, i = t.to_check.begin();
                  i != t.to_check.end(); i = j ) {
                j = i; ++j;
                if ( t.extension ( *i ).done )
                    t.to_check.erase( i );
            }
        }
    };

    template< typename Algorithm >
    void _porEliminate( Algorithm &a ) {
        typedef POREliminate< typename Algorithm::Setup > Elim;
        visitor::Partitioned< Elim, Algorithm >
            visitor( *this, a, *this, a.store() );

        Elim::init( *this, visitor );
        visitor.processQueue();
        Elim::cleanup( *this );
    }

    // gives true iff there are nodes that need expanding
    template< typename Domain, typename Alg >
    bool porEliminate( Domain &d, Alg &a ) {
        to_expand.clear();
        while ( !to_check.empty() ) {
            d.parallel( &Alg::_por_worker );

            if ( to_check.empty() )
                break;

            updatePredCount( *to_check.begin(), 0 );
            to_expand.insert( *to_check.begin() );
        }

        return to_expand.size() > 0;
    }

    template< typename Algorithm >
    bool porEliminateLocally( Algorithm &a ) {
        typedef POREliminate< typename Algorithm::Setup > Elim;
        visitor::BFV< Elim > visitor( *this, *this, a.store() );

        to_expand.clear();
        while ( !to_check.empty() ) {
            Elim::init( *this, visitor );
            visitor.processQueue();
            Elim::cleanup( *this );

            if ( to_check.empty() )
                break;

            // break a stalemate
            updatePredCount( *to_check.begin(), 0 );
            to_expand.insert( *to_check.begin() );
        }

        return to_expand.size() > 0;
    }

    template< typename Yield >
    void initials( Yield yield ) {
        if ( to_expand.size() == 0 )
            this->base().initials( yield );
        else {
            for ( auto i : to_expand )
                fullexpand( yield, i );
        }
    }

    template< typename Yield >
    void fullexpand( Yield yield, Node n ) {
        extension( n ).full = true;
        std::set< std::pair< Node, Label > > all, ample, out;
        std::vector< std::pair< Node, Label > > extra;

        this->base().successors( n, [&]( Node x, Label l ) { all.insert( std::make_pair( x, l ) ); } );
        this->base().ample( n, [&]( Node x, Label l ) { ample.insert( std::make_pair( x, l ) ); } );

        std::set_difference( all.begin(), all.end(), ample.begin(), ample.end(),
                             std::inserter( out, out.begin() ) );

        // accumulate all the extra states we have generated
        std::copy( ample.begin(), ample.end(), std::back_inserter( extra ) );
        std::set_intersection( all.begin(), all.end(), ample.begin(), ample.end(),
                               std::back_inserter( extra ) );

        // release the states that we aren't going to use
        for ( auto i = extra.begin(); i != extra.end(); ++i )
            this->base().release( i->first );

        for ( auto i = out.begin(); i != out.end(); ++i ) {
            const_cast< Blob* >( &i->first )->header().permanent = 1;
            yield( n, i->first, i->second );
        }
    }
};

}
}

#endif
