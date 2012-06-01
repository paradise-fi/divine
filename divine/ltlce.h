// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

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
    bool current_updated;
};

template< typename Node >
static inline bitstream &operator<<( bitstream &bs, const CeShared< Node > &sh )
{
    return bs << sh.initial << sh.current << sh.successor << sh.successorPos
              << sh.current_updated;
}

template< typename Node >
static inline bitstream &operator>>( bitstream &bs, CeShared< Node > &sh )
{
    return bs >> sh.initial >> sh.current >> sh.successor >> sh.successorPos
              >> sh.current_updated;
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
    int owner( Hash &hash, Worker &worker, Node n ) {
        return g().owner( hash, worker, n );
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
            o_ce( a ) << _g.showNode( trace.back() ) << std::endl;
            _g.release( trace.back() );
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
                shared().ce.current = Node(); // owned by trace now
            }
        }


        return std::make_pair( trace, numTrace );
    }

    template< typename Domain, typename Alg >
    void linear( Domain &d, Alg &a )
    {
        generateLinear( a, g(), parentTrace( d, a, g().initial() ) );
    }

    template< typename Domain, typename Alg >
    void lasso( Domain &d, Alg &a ) {
        linear( d, a );
        ++ shared().iteration;
        d.parallel( &Alg::_traceCycle );

        generateLasso( a, g(), parentTrace( d, a, shared().ce.initial ) );
    }

};

}
}

#endif
