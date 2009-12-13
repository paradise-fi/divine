// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <sstream>

#ifndef DIVINE_LTLCE_H
#define DIVINE_LTLCE_H

namespace divine {
namespace algorithm {

template< typename Node >
struct CeShared {
    Node initial;
    Node current;
    bool current_updated;
    template< typename I >
    I read( I i ) {
        FakePool fp;

        if ( *i++ )
            i = initial.read32( &fp, i );
        if ( *i++ )
            i = current.read32( &fp, i );
        current_updated = *i++;
        return i;
    }

    template< typename O >
    O write( O o ) {
        *o++ = initial.valid();
        if ( initial.valid() )
            o = initial.write32( o );
        *o++ = current.valid();
        if ( current.valid() )
            o = current.write32( o );
        *o++ = current_updated;
        return o;
    }
};

template< typename G, typename Shared, typename Extension >
struct LtlCE {
    typedef typename G::Node Node;
    typedef LtlCE< G, Shared, Extension > Us;

    G *_g;
    Shared *_shared;

    G &g() { assert( _g ); return *_g; }
    Shared &shared() { assert( _shared ); return *_shared; }

    LtlCE() : _g( 0 ), _shared( 0 ) {}

    // XXX duplicated from visitor
    template< typename Hash, typename Worker >
    int owner( Hash &hash, Worker &worker, Node n ) const {
        return hash( n ) % worker.peers();
    }

    void setup( G &g, Shared &s )
    {
        _g = &g;
        _shared = &s;
    }

    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    bool updateIteration( Node t ) {
        int old = extension( t ).iteration;
        extension( t ).iteration = shared().iteration;
        return old != shared().iteration;
    }

    // -------------------------------------
    // -- Parent trace extraction
    // --

    template< typename Worker, typename Hasher, typename Equal, typename Table >
    void _parentTrace( Worker &w, Hasher &h, Equal &eq, Table &t ) {
        if ( shared().ce.current_updated )
            return;
        if ( owner( h, w, shared().ce.current ) == w.globalId() ) {
            Node n = t.get( shared().ce.current ).key;
            assert( n.valid() );
            shared().ce.current = extension( n ).parent;
            assert( shared().ce.current.valid() );
            shared().ce.current_updated = true;
        }
    }

    // -------------------------------------
    // -- Local cycle discovery
    // --

    visitor::ExpansionAction cycleExpansion( Node n ) {
        return visitor::ExpandState;
    }

    visitor::TransitionAction cycleTransition( Node from, Node to ) {
        if ( from.valid() && to == shared().ce.initial ) {
            extension( to ).parent = from;
            return visitor::TerminateOnTransition;
        }
        if ( updateIteration( to ) ) {
            extension( to ).parent = from;
            return visitor::ExpandTransition;
        }
        return visitor::ForgetTransition;
    }

    template< typename Worker, typename Hasher, typename Table >
    void _traceCycle( Worker &w, Hasher &h, Table &t ) {
        typedef visitor::Setup< G, Us, Table,
            &Us::cycleTransition,
            &Us::cycleExpansion > Setup;
        typedef visitor::Parallel< Setup, Worker, Hasher > Visitor;

        Visitor visitor( g(), w, *this, h, &t );
        assert( shared().ce.initial.valid() );
        if ( visitor.owner( shared().ce.initial ) == w.globalId() ) {
            shared().ce.initial = t.get( shared().ce.initial ).key;
            visitor.queue( Blob(), shared().ce.initial );
        }
        visitor.processQueue();
    }

    // ------------------------------------------------
    // -- Lasso counterexample generation
    // --

    template< typename Domain, typename Alg >
    std::string parentTrace( Domain &d, Alg &a, Node stop, bool cycle = 0 ) {
        std::vector< Node > trace;
        std::stringstream o_tr_str;
        std::ostream &o_ce = a.config().ceStream(),
                     &o_tr = a.config().trailStream();
        shared().ce.current = shared().ce.initial;
        do {
            shared().ce.current_updated = false;
            trace.push_back( shared().ce.current );
            d.parallel().runInRing( shared(), &Alg::_parentTrace );
            assert( shared().ce.current_updated );
        } while ( !a.equal( shared().ce.current, stop ) );
        trace.push_back( shared().ce.current );

        while ( !trace.empty() ) {
            Node current = trace.back();
            trace.pop_back();
            o_ce << g().showNode( current ) << std::endl;
            if ( trace.empty() ) {
                g().release( current );
                break;
            }

            typename G::Successors succ = g().successors( current );
            int edge = 0;
            while ( !succ.empty() ) {
                ++ edge;
                if ( a.equal( succ.head(), trace.back() ) )
                    break;
                succ = succ.tail();
            }
            assert_leq( 1, edge );
            o_tr << edge << std::endl;
            o_tr_str << edge << ",";

            g().release( current );
        }
        // drop trailing comma
        return std::string( o_tr_str.str(), 0, o_tr_str.str().length() - 1 );
    }

    template< typename Domain, typename Alg >
    void linear( Domain &d, Alg &a ) {
        a.config().ceStream() << std::endl << "===== Trace from initial ====="
                              << std::endl << std::endl;
        a.config().trailStream() << "# from initial" << std::endl;
        a.result().iniTrail = parentTrace( d, a, g().initial(), false );
    }

    template< typename Domain, typename Alg >
    void lasso( Domain &d, Alg &a ) {
        linear( d, a );
        ++ shared().iteration;
        d.parallel().run( shared(), &Alg::_traceCycle );

        a.config().ceStream() << std::endl << "===== The cycle ====="
                              << std::endl << std::endl;
        a.config().trailStream() << "# cycle" << std::endl;
        a.result().cycleTrail = parentTrace( d, a, shared().ce.initial, true );
    }

};

}
}

#endif
