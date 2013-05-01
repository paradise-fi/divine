// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>
//             (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <stdint.h>
#include <sstream>
#include <utility>

#ifndef DIVINE_LTLCE_H
#define DIVINE_LTLCE_H

namespace divine {
namespace algorithm {

template< typename Node, typename Handle >
struct CeShared {
    Handle initial;
    Handle current;
    Handle successor;
    bool current_updated;
    bool is_ce_initial;
    int successor_id;
    Node parent;
};

template< typename BS, typename Node, typename Handle >
typename BS::bitstream &operator<<( BS &bs, const CeShared< Node, Handle > &sh )
{
    return bs << sh.initial << sh.current << sh.successor << sh.current_updated
              << sh.successor_id << sh.parent;
}

template< typename BS, typename Node, typename Handle >
typename BS::bitstream &operator>>( BS &bs, CeShared< Node, Handle > &sh )
{
    return bs >> sh.initial >> sh.current >> sh.successor >> sh.current_updated
              >> sh.successor_id >> sh.parent;
}

template< typename _Setup, typename Shared, typename Extension, typename Hasher >
struct LtlCE {
    typedef typename _Setup::Graph G;
    typedef typename G::Node Node;
    typedef typename G::Label Label;
    using Store = typename _Setup::Store;
    using Handle = typename Store::Handle;
    using Vertex = typename Store::Vertex;

    typedef LtlCE< _Setup, Shared, Extension, Hasher > This;
    typedef CeShared< Node, Handle > ThisCeShared;

    G *_g;
    Shared *_shared;
    Store *_store;

    G &g() { assert( _g ); return *_g; }
    Shared &shared() { assert( _shared ); return *_shared; }

    template< typename Algorithm >
    LtlCE( Algorithm *a ) : LtlCE() { setup( *a ); }

    LtlCE() : _g( 0 ), _shared( 0 ), _store( nullptr ) {}
    ~LtlCE() {}

    template< typename Algorithm >
    void setup( Algorithm &a, Shared &s )
    {
        setup( a );
        _shared = &s;
    }

    template< typename Algorithm >
    void setup( Algorithm &a )
    {
        _g = &a.graph();
        _store = &a.store();
    }

    Store &store() { assert( _store ); return *_store; }
    Pool& pool() {
        return g().pool();
    }

    Extension& extension( Vertex vi ) {
        return vi.template extension< Extension >();
    }

    bool updateIteration( Vertex t ) {
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
        assert( s.valid( shared().ce.current ) );

        Handle h = shared().ce.current;
        if ( s.knows( h ) ) {
            shared().ce.successor = h;
            shared().ce.current = extension( store().vertex( h ) ).parent;
            shared().ce.current_updated = true;
            Handle init = shared().ce.initial;
            shared().ce.is_ce_initial = s.equal( init, h );
        }
    }

    template < typename Worker, typename Store >
    void _ceIsInitial( Worker& w, Store& s ) {
        if ( shared().ce.current_updated )
            return;
        if ( !s.valid( shared().ce.current ) )
            return;
        if ( s.knows( shared().ce.current ) ) {
            shared().ce.successor_id = whichInitial( shared().ce.current, s );
            shared().ce.current_updated = shared().ce.successor_id > 0;
        }
    }

    template < typename Worker, typename Store >
    void _successorTrace( Worker& w, Store& s ) {
        if ( shared().ce.current_updated )
            return;
        assert( s.valid( shared().ce.parent ) );

        if ( !s.knows( shared().ce.current ) )
            return;

        int succnum = 0;
        Node parent = shared().ce.parent;
        g().allSuccessors(
            parent, [ this, &s, &succnum, &w ]( Node n, Label ) {
                if ( this->shared().ce.current_updated )
                    return;
                ++succnum;
                if ( s.equal( n, s.vertex( this->shared().ce.current ).node() ) ) {
                    this->shared().ce.parent = n;
                    this->shared().ce.successor_id = succnum;
                    this->shared().ce.current_updated = true;
                } else
                    this->g().release( n );
            } );
        shared().ce.current_updated = true;
    }

    int whichInitial( Node n ) {
        int res = 0, i = 0;
        g().initials( [&]( Node, Node o, Label ) {
                ++ i;
                if ( this->store().equal( n, o ) )
                    res = i;
                this->g().release( o ); /* leaving out this-> trips an ICE */
            } );
        return res;
    }

    template < typename Store >
    int whichInitial( Handle h, Store& s ) {
        return whichInitial( s.vertex( h ).node() );
    }

    Node getInitialById( int id ) {
        int i = 0;
        Node init;
        g().initials( [ this, id, &i, &init ]( Node, Node o, Label ) {
                ++i;
                if ( i == id )
                    init = o;
                else
                    this->g().release( o );
            } );
        return init;
    }


    // -------------------------------------
    // -- Local cycle discovery
    // --

    template< typename Setup >
    struct FindCycle : algorithm::Visit< This, Setup >
    {

        static visitor::ExpansionAction expansion( This &, Vertex n ) {
            return visitor::ExpansionAction::Expand;
        }

        static visitor::TransitionAction transition( This &t, Vertex from, Vertex to, Label ) {
            if ( !t.store().valid( from ) )
                return visitor::TransitionAction::Expand;
            if ( t.store().valid( from ) && t.whichInitial( to.node() ) ) {
                t.extension( to ).parent = from.handle();
                return visitor::TransitionAction::Terminate;
            }
            if ( t.updateIteration( to ) ) {
                t.extension( to ).parent = from.handle();
                return visitor::TransitionAction::Expand;
            }
            return visitor::TransitionAction::Forget;
        }
    };

    template< typename Algorithm >
    void _traceCycle( Algorithm &a ) {
        typedef FindCycle< typename Algorithm::Setup > Find;
        typename Find::Visitor::template Implementation< Find, Algorithm >
            visitor( *this, a, a.graph(), a.store(), a.data );

        assert( a.store().valid( shared().ce.parent ) );
        if ( a.store().knows( shared().ce.parent ) ) {
            assert( a.store().knows( shared().ce.initial ) );
            Vertex parent = a.store().fetch( shared().ce.parent );
            shared().ce.parent = parent.node();
            // since initial must be parent's handle
            assert( a.store().equal( parent.handle(), shared().ce.initial ) );
            visitor.queue( Vertex(), parent.node(), Label() );
            parent.disown();
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
            if ( pool().valid( trace.back() ) ) {
                o_ce( a ) << _g.showNode( trace.back() ) << std::endl;
                // _g.release( trace.back() );
            } else {
                o_ce( a ) << "?" << std::endl;
            }
            trace.pop_back();
        }

        for ( unsigned next : ntrace ) {
            o_tr_str << next << ",";
        }

        // drop trailing comma
        return std::string( o_tr_str.str(), 0, o_tr_str.str().length() - 1 );
    }

    template< typename Alg >
    std::string generateTrace( Alg &a, G &_g, Traces traces ) {
        return generateTrace( a, _g, traces.first, &traces.second );
    }

    inline Node traceBack( Traces& tr ) {
        return tr.first.empty() ? Node() : tr.first.back();
    }

    template < typename T >
    inline Node traceBack( T& tr ) {
        return tr.empty() ? Node() : tr.back();
    }

    template< typename Alg, typename T >
    Node generateLinear( Alg &a, G &_g, T trace ) {
        o_ce( a ) << std::endl << "===== Trace from initial =====" << std::endl << std::endl;
        a.result().iniTrail = generateTrace( a, _g, trace );
        return traceBack( trace );
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

    enum class TraceType {
        Linear,
        Lasso
    };
    using Linear = std::integral_constant< TraceType, TraceType::Linear >;
    using Lasso = std::integral_constant< TraceType, TraceType::Lasso >;

    template< typename Domain, typename Alg, typename TT >
    Traces parentTrace( Domain &d, Alg &a, TT traceType ) {
        return parentTrace< Domain, Alg, TT >( d, a );
    }


    template< typename Domain, typename Alg, typename TT >
    Traces parentTrace( Domain &d, Alg &a ) {
        Node parent = shared().ce.parent;
        std::deque< Handle > hTrace;

        // trace backward to initial using handles
        shared().ce.current = shared().ce.initial;
        hTrace.push_front( shared().ce.initial );
        shared().ce.parent = Node();
        shared().ce.successor = Handle(); // !valid()
        shared().ce.successor_id = 0;
        bool first = false;
        while ( a.store().valid( shared().ce.current ) ) {
            shared().ce.current_updated = shared().ce.is_ce_initial = false;
            d.ring( &Alg::_parentTrace );
            assert( shared().ce.current_updated );
            hTrace.push_front( shared().ce.current );

            if ( TT::value == TraceType::Linear ) {
                shared().ce.successor_id = 0;
                shared().ce.current_updated = false;
                d.ring( &Alg::_ceIsInitial );
                if ( shared().ce.current_updated )
                    break;
            } else if ( TT::value == TraceType::Lasso ) {
                if ( shared().ce.is_ce_initial ) {
                    if ( first )
                        break;
                    else
                        first = true;
                }
            }
            else
                assert_die();
        }
        hTrace.pop_front(); // initial

        return succTrace< Domain, Alg, decltype( hTrace.begin() ), TT >(
                d, a, parent, hTrace.begin(), hTrace.end() );
    }


    template< typename Domain, typename Alg, typename Iter, typename TT >
    Traces succTrace( Domain &d, Alg &a, TT traceType, Node parent,
            Iter hTraceBegin, Iter hTraceEnd )
    {
        return succTrace< Domain, Alg, Iter, TT >( d, a, parent, hTraceBegin,
                hTraceEnd );
    }

    template< typename Domain, typename Alg, typename Iter, typename TT >
    Traces succTrace( Domain &d, Alg &a, Node parent, Iter hTraceBegin,
                      Iter hTraceEnd )
    {
        // track forward by handles, generating full traces
        Trace trace;
        NumericTrace numTrace;

        switch ( TT::value ) {
            case TraceType::Linear: {
                if ( shared().ce.successor_id == 0 ) // empty CE
                    return std::make_pair( trace, numTrace );
                Node initial = getInitialById( shared().ce.successor_id );
                assert( a.pool().valid( initial ) );
                shared().ce.parent = initial;
                trace.push_back( initial );
                numTrace.push_back( shared().ce.successor_id );
                break; }
            case TraceType::Lasso: {
                ++hTraceBegin;
                shared().ce.parent = parent;
                break; }
            default:
                assert_die();
        }


        assert( a.store().valid( shared().ce.parent ) );
        shared().ce.successor = Handle();
        while ( hTraceBegin != hTraceEnd ) {
            shared().ce.current_updated = false;
            shared().ce.successor_id = 0;
            shared().ce.current = *hTraceBegin;
            ++hTraceBegin;
            d.ring( &Alg::_successorTrace );
            assert( shared().ce.current_updated );
            trace.push_back( shared().ce.parent );
            numTrace.push_back( shared().ce.successor_id );
        }

        shared().ce.current = Handle();
        shared().ce.successor = Handle();

        return std::make_pair( trace, numTrace );
    }

    template< typename Alg, typename Iter, typename TT >
    Traces succTraceLocal( Alg &a, TT traceType, Node parent,
                           Iter hTraceBegin, Iter hTraceEnd )
    {
        // track forward by handles, generating full traces
        Trace trace;
        NumericTrace numTrace;

        if ( TT::value == TraceType::Linear ) {
            if ( hTraceBegin == hTraceEnd ) // empty CE
                return std::make_pair( trace, numTrace );
            int i = 0;
            bool done = false;
            a.graph().initials( [&]( Node, Node o, Label ) {
                    if ( done )
                        return;
                    ++i;
                    Vertex v = a.store().fetch( o );
                    if ( a.store().equal( v.handle(), *hTraceBegin ) ) {
                        parent = o;
                        done = true;
                    }
                } );
            assert( a.pool().valid( parent ) );
            trace.push_back( parent );
            numTrace.push_back( i );
        } else
            assert( TT::value == TraceType::Lasso );

        assert( a.store().valid( parent ) );
        Node successor;
        for ( ++hTraceBegin; hTraceBegin != hTraceEnd; ++hTraceBegin ) {
            int i = 0;
            bool done = false;
            a.graph().allSuccessors( parent, [&]( Node t, Label ) {
                    if ( done )
                        return;
                    ++i;
                    Vertex v = a.store().fetch( t );
                    if ( a.store().equal( v.handle(), *hTraceBegin ) ) {
                        parent = t;
                        done = true;
                    }
                } );
            assert( done );
            trace.push_back( parent );
            numTrace.push_back( i );
        }

        return std::make_pair( trace, numTrace );
    }

    template< typename Domain, typename Alg >
    Node linear( Domain &d, Alg &a )
    {
        return generateLinear( a, g(),
                parentTrace( d, a, Linear() ) );
    }

    template< typename Domain, typename Alg >
    void lasso( Domain &d, Alg &a )
    {
        Node cycleNode = linear( d, a );
        assert( a.store().valid( cycleNode ) );
        shared().ce.parent = cycleNode;
        ++ shared().iteration;
        d.parallel( &Alg::_traceCycle );
        generateLasso( a, g(),
                parentTrace( d, a, Lasso() ) );
    }
};

}
}

#endif
