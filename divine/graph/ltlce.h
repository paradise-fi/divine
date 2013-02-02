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

template< typename G, typename Shared, typename Extension, typename Hasher >
struct LtlCE {
    typedef typename G::Node Node;
    typedef typename G::Label Label;
    typedef LtlCE< G, Shared, Extension, Hasher > This;

    G *_g;
    Shared *_shared;
    Hasher *_hasher;

    G &g() { assert( _g ); return *_g; }
    Shared &shared() { assert( _shared ); return *_shared; }

    LtlCE() : _g( 0 ), _shared( 0 ), _hasher( nullptr ) {}
    ~LtlCE() {}

    void setup( G &g, Shared &s, Hasher &h )
    {
        _g = &g;
        _shared = &s;
        _hasher = &h;
    }

    bool equal( Node a, Node b ) {
        assert( _hasher );
        return _hasher->equal( a, b );
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

    template< typename Worker, typename Store >
    void _parentTrace( Worker &w, Store &s ) {
        if ( shared().ce.current_updated )
            return;
        if ( s.owner( w, shared().ce.current ) == w.id() ) {
            Node n = s.fetch( shared().ce.current, s.hash( shared().ce.current ) );
            assert( n.valid() );

            shared().ce.successor = n;
            shared().ce.current = extension( n ).parent;
            shared().ce.current_updated = true;

            shared().ce.successorPos =
                g().successorNum( w, shared().ce.current, shared().ce.successor );
        }
    }

    int whichInitial( Node n ) {
        int res = 0, i = 0;
        g().initials( [&]( Node, Node o, Label ) {
                ++ i;
                if ( this->equal( n, o ) )
                    res = i;
                this->g().release( o ); /* leaving out this-> trips an ICE */
            } );
        return res;
    }

    // -------------------------------------
    // -- Local cycle discovery
    // --

    template< typename Setup >
    struct FindCycle : algorithm::Visit< This, Setup >
    {

        static visitor::ExpansionAction expansion( This &, Node n ) {
            return visitor::ExpandState;
        }

        static visitor::TransitionAction transition( This &t, Node from, Node to, Label ) {
            if ( from.valid() && t.whichInitial( to ) ) {
                t.extension( to ).parent = from;
                return visitor::TerminateOnTransition;
            }
            if ( t.updateIteration( to ) ) {
                t.extension( to ).parent = from;
                return visitor::ExpandTransition;
            }
            return visitor::ForgetTransition;
        }
    };

    template< typename Algorithm >
    void _traceCycle( Algorithm &a ) {
        typedef FindCycle< typename Algorithm::Setup > Find;
        visitor::Partitioned< Find, Algorithm >
            visitor( *this, a, a.graph(), a.store() );

        assert( shared().ce.initial.valid() );
        if ( a.store().owner( a, shared().ce.initial ) == a.id() ) {
            shared().ce.initial = a.store().fetch( shared().ce.initial,
                                                   a.store().hash( shared().ce.initial ) );
            visitor.queue( Blob(), shared().ce.initial, Label() );
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

    template < typename A >
    std::ostream &o_ce( A &a )
    {
        static struct : std::streambuf {} buf;
        static std::ostream null(&buf);
        return a.meta().output.displayCe ? std::cerr : null;
    }

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
        a.result().iniTrail = generateTrace( a, _g, trace );
    }

    template< typename Alg, typename T >
    void generateLasso( Alg &a, G &_g, T trace ) {
        o_ce( a ) << std::endl << "===== The cycle =====" << std::endl << std::endl;
        a.result().cycleTrail = generateTrace( a, _g, trace );
    }

    template< typename Alg, typename T >
    void goal( Alg &a, T goal ) {
        o_ce( a ) << std::endl << "===== The goal =====" << std::endl << std::endl;

        std::vector< T > trace;
        trace.push_back( goal );
        a.result().cycleTrail = generateTrace( a, g(), trace );
    }

    // ------------------------------------------------
    // -- Lasso counterexample generation
    // --

    template< typename Domain, typename Alg, typename Stop >
    Traces parentTrace( Domain &d, Alg &a, Stop stop ) {
        Trace trace;
        NumericTrace numTrace;
        shared().ce.current = shared().ce.initial;
        shared().ce.successor = Node(); // !valid()
        shared().ce.successorPos = 0;
        while ( shared().ce.current.valid() ) {
            auto stopped = stop( shared().ce.current );
            if ( stopped.first && !numTrace.empty() ) {
                if ( stopped.second )
                    numTrace.push_back( stopped.second );
                break;
            }
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
    void linear( Domain &d, Alg &a )
    {
        generateLinear( a, g(), parentTrace(
                            d, a, [this]( Node n ) {
                                return std::make_pair( bool( this->whichInitial( n ) ),
                                                       this->whichInitial( n ) );
                            } ) );
    }

    template< typename Domain, typename Alg >
    void lasso( Domain &d, Alg &a )
    {
        linear( d, a );
        ++ shared().iteration;
        d.parallel( &Alg::_traceCycle );
        generateLasso(
            a, g(), parentTrace(
                d, a, [this]( Node n ) {
                    return std::make_pair(
                        this->equal( n, this->shared().ce.initial ),
                        0 );
                } ) );
    }
};

}
}

#endif
