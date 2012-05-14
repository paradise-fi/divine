// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <stdint.h>
#include <sstream>
#include <utility>

#ifndef DIVINE_LTLCE_H
#define DIVINE_LTLCE_H

namespace divine {
namespace algorithm {

template< typename Node >
struct CeShared {
    Node initial;
    Node current;
    Node successor;
    unsigned successorPos;
    hash_t current_hash;
    bool current_updated;
};

template< typename BS, typename Node >
typename BS::bitstream &operator<<( BS &bs, const CeShared< Node > &sh )
{
    return bs << sh.initial << sh.current << sh.successor << sh.successorPos
              << sh.current_hash << sh.current_updated;
}

template< typename BS, typename Node >
typename BS::bitstream &operator>>( BS &bs, CeShared< Node > &sh )
{
    return bs >> sh.initial >> sh.current >> sh.successor >> sh.successorPos
              >> sh.current_hash >> sh.current_updated;
}

template< typename G, typename Shared, typename Extension >
struct LtlCE {
    typedef typename G::Node Node;
    typedef LtlCE< G, Shared, Extension > Us;

    G *_g;
    Shared *_shared;

    std::ostream *_o_ce, *_o_trail;

    G &g() { assert( _g ); return *_g; }
    Shared &shared() { assert( _shared ); return *_shared; }

    LtlCE() : _g( 0 ), _shared( 0 ), _o_ce( 0 ), _o_trail( 0 ) {}
    ~LtlCE() {
        if ( ( _o_ce != &std::cerr ) && ( _o_ce != &std::cout ) )
            divine::safe_delete( _o_ce );
        if ( ( _o_trail != &std::cerr ) && ( _o_trail != &std::cout ) )
            divine::safe_delete( _o_trail );
    }

    template< typename Hash, typename Worker >
    int owner( Hash &hash, Worker &worker, Node n, hash_t hint = 0 ) {
        return g().owner( hash, worker, n, hint );
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
    void _parentTrace( Worker &w, Hasher &h, Equal &, Table &t ) {
        if ( shared().ce.current_updated )
            return;
        if ( owner( h, w, shared().ce.current ) == w.id() ) {
            Node n = t.get( shared().ce.current );
            assert( n.valid() );

            shared().ce.successor = n;
            shared().ce.current = extension( n ).parent;
            shared().ce.current_updated = true;

            shared().ce.successorPos =
                g().successorNum( w, shared().ce.current, shared().ce.successor );
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
        typedef visitor::Setup< G, Us, Table, NoStatistics,
            &Us::cycleTransition,
            &Us::cycleExpansion > Setup;
        typedef visitor::Partitioned< Setup, Worker, Hasher > Visitor;

        Visitor visitor( g(), w, *this, h, &t );
        assert( shared().ce.initial.valid() );
        if ( visitor.owner( shared().ce.initial ) == w.id() ) {
            shared().ce.initial = t.get( shared().ce.initial );
            visitor.queue( Blob(), shared().ce.initial );
        }
        visitor.processQueue();
    }

    // Obtaining CE output

    typedef std::vector< unsigned > NumericTrace;
    typedef std::vector< Node > Trace;
    typedef std::pair< Trace, NumericTrace > Traces;

    template< typename Alg, typename T >
    NumericTrace numericTrace( Alg &a, G &_g, T trace )
    {
        NumericTrace result;
        while ( !trace.empty() ) {
            Node current = trace.back();
            trace.pop_back();

            if ( trace.empty() )
                break;

            result.push_back( _g.successorNum( a, current, trace.back() ) );
        }
        return result;
    }

    std::ostream &filestream( std::ostream *&stream, std::string file ) {
        if ( !stream ) {
            if ( file.empty() )
                stream = new std::stringstream();
            else if ( file == "-" )
                stream = &std::cerr;
            else
                stream = new std::ofstream( file.c_str() );
        }
        return *stream;
    }

    template < typename A >
    std::ostream &o_ce( A &a ) { return filestream( _o_ce, a.meta().output.ceFile ); }
    template < typename A >
    std::ostream &o_trail( A &a ) { return filestream( _o_trail, a.meta().output.trailFile ); }

    /// Generates traces; when numTrace is null, computes a numeric trace sequentially in a single thread
    template< typename Alg, typename T >
    std::string generateTrace( Alg &a, G &_g, T trace, NumericTrace *numTrace = NULL ) {
        std::stringstream o_tr_str;

        NumericTrace ntrace = numTrace ? *numTrace : numericTrace( a, _g, trace );

        while ( !trace.empty() ) {
            if ( trace.back().valid() ) {
                o_ce( a ) << _g.showNode( trace.back() ) << std::endl;
                _g.release( trace.back() );
            } else {
                o_ce( a ) << "?" << std::endl;
            }
            trace.pop_back();
        }

        for ( int i = ntrace.size() - 1; i >= 0; --i ) {
            o_trail( a ) << ntrace[ i ] << std::endl;
            o_tr_str << ntrace[ i ] << ",";
        }

        // drop trailing comma
        return std::string( o_tr_str.str(), 0, o_tr_str.str().length() - 1 );
    }

    template< typename Alg >
    std::string generateTrace( Alg &a, G &_g, Traces traces ) {
        return generateTrace( a, _g, traces.first, &traces.second );
    }

    template< typename Alg, typename T >
    void generateLinear( Alg &a, G &_g, T trace ) {
        o_ce( a ) << std::endl << "===== Trace from initial =====" << std::endl << std::endl;
        o_trail( a ) << "# from initial" << std::endl;
        a.result().iniTrail = generateTrace( a, _g, trace );
    }

    template< typename Alg, typename T >
    void generateLasso( Alg &a, G &_g, T trace ) {
        o_ce( a ) << std::endl << "===== The cycle =====" << std::endl << std::endl;
        o_trail( a ) << "# cycle" << std::endl;
        a.result().cycleTrail = generateTrace( a, _g, trace );
    }

    template< typename Alg, typename T >
    void goal( Alg &a, T goal ) {
        o_ce( a ) << std::endl << "===== The goal =====" << std::endl << std::endl;
        o_trail( a ) << "# goal" << std::endl;

        std::vector< T > trace;
        trace.push_back( goal );
        a.result().cycleTrail = generateTrace( a, g(), trace );
    }

    // ------------------------------------------------
    // -- Lasso counterexample generation
    // --

    template< typename Domain, typename Alg >
    Traces parentTrace( Domain &d, Alg &a, Node stop ) {
        Trace trace;
        NumericTrace numTrace;
        shared().ce.current = shared().ce.initial;
        shared().ce.successor = Node(); // !valid()
        shared().ce.successorPos = 0;
        while ( shared().ce.current.valid() ) {
            if ( a.equal( shared().ce.current, stop ) && !numTrace.empty() )
                break;
            shared().ce.current_updated = false;
            d.ring( &Alg::_parentTrace );
            assert( shared().ce.current_updated );
            if ( shared().ce.successorPos ) {
                trace.push_back( shared().ce.current );
                numTrace.push_back( shared().ce.successorPos );
            }
        }

        shared().ce.current = Node();
        shared().ce.successor = Node();

        return std::make_pair( trace, numTrace );
    }

    template< typename Domain, typename Alg >
    void linear( Domain &d, Alg &a, Traces (Us::*traceFnc)( Domain&, Alg&, Node ) = &Us::parentTrace<Domain, Alg> )
    {
        generateLinear( a, g(), (this->*traceFnc)( d, a, g().initial() ) );
    }

    template< typename Domain, typename Alg >
    void lasso( Domain &d, Alg &a, Traces (Us::*traceFnc)( Domain&, Alg&, Node ) = &Us::parentTrace<Domain, Alg> ) {
        linear( d, a, traceFnc );
        ++ shared().iteration;
        d.parallel( &Alg::_traceCycle );

        generateLasso( a, g(), (this->*traceFnc)( d, a, shared().ce.initial ) );
    }

    // ------------------------------------------------
    // -- Hash compaction counter-examples
    // --

    template< typename Worker, typename Hasher, typename Equal, typename Table >
    void _hashTrace( Worker &w, Hasher &h, Equal &, Table &t ) {
        if ( shared().ce.current_updated )
            return;
        if ( owner( h, w, Node(), shared().ce.current_hash ) == w.id() ) { // determine owner just from hash
            Node n = t.getHinted( Node(), shared().ce.current_hash );
            if ( n.valid() ) {
                shared().ce.current_hash = hash_t( uintptr_t( extension( n ).parent.ptr ) );
                shared().ce.current_updated = true;
            }
        }
    }

    template< typename Domain, typename Alg >
    Traces hashTrace( Domain &d, Alg &a, Node stop ) {
        Trace trace;
        std::vector< hash_t > hashes;
        NumericTrace numTrace;
        // construct trail of hashes
        shared().ce.current = Node(); // not tracking nodes
        shared().ce.current_hash = a.hasher( shared().ce.initial );
        hash_t stop_hash = a.hasher( stop );
        while ( true ) {
            if ( shared().ce.current_hash == stop_hash && !hashes.empty() )
                break;
            shared().ce.current_updated = false;
            d.ring( &Alg::_hashTrace );
            if ( !shared().ce.current_updated )
                break;
            hashes.push_back( shared().ce.current_hash );
        }
        // follow the trail from the _stop_ state
        Node cur = stop;
        while ( !hashes.empty() ) {
            std::pair< int, Node > ret = g().successorNumHash( a, cur, hashes.back() );
            if ( ret.first > 0 ) {
                trace.push_back( ret.second );
                numTrace.push_back( ret.first );
                cur = ret.second;
            } else {
                trace.push_back( Node() );
                numTrace.push_back( 0 );
            }
            hashes.pop_back();
        }

        // reverse the trace
        std::reverse( trace.begin(), trace.end() );
        std::reverse( numTrace.begin(), numTrace.end() );

        return std::make_pair( trace, numTrace );
    }

    template< typename Domain, typename Alg >
    void linear_hc( Domain &d, Alg &a ) {
        linear( d, a, &Us::hashTrace<Domain, Alg> );
    }

    template< typename Domain, typename Alg >
    void lasso_hc( Domain &d, Alg &a ) {
        lasso( d, a, &Us::hashTrace<Domain, Alg> );
    }


};

}
}

#endif
