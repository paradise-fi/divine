// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>
//             (c) 2013,2014 Vladimír Štill <xstill@fi.muni.cz>

#include <cstdint>
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
              << sh.is_ce_initial << sh.successor_id << sh.parent;
}

template< typename BS, typename Node, typename Handle >
typename BS::bitstream &operator>>( BS &bs, CeShared< Node, Handle > &sh )
{
    return bs >> sh.initial >> sh.current >> sh.successor >> sh.current_updated
              >> sh.is_ce_initial >> sh.successor_id >> sh.parent;
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
    Pool& pool() { return g().pool(); }

    Extension& extension( Vertex vi ) {
        return vi.template extension< Extension >();
    }

    bool updateIteration( Vertex t ) {
        int old = extension( t ).iteration();
        extension( t ).iteration() = shared().iteration;
        return old != shared().iteration;
    }

    // -------------------------------------
    // -- Parent trace extraction
    // --

    template< typename Worker, typename Store >
    void _parentTrace( Worker &, Store &s ) {
        if ( shared().ce.current_updated )
            return;
        assert( s.valid( shared().ce.current ) );

        Handle h = shared().ce.current;
        if ( s.knows( h ) ) {
            shared().ce.successor = h;
            shared().ce.current = extension( store().vertex( h ) ).parent();
            shared().ce.current_updated = true;
            Handle init = shared().ce.initial;
            shared().ce.is_ce_initial = s.equal( init, h );
        }
    }

    template < typename Worker, typename Store >
    void _ceIsInitial( Worker &, Store& s ) {
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
    void _successorTrace( Worker &, Store& s ) {
        if ( shared().ce.current_updated )
            return;
        assert( s.valid( shared().ce.parent ) );

        if ( !s.knows( shared().ce.current ) )
            return;

        int succnum = 0;
        Node parent = shared().ce.parent;
        g().allSuccessors(
            parent, [ this, &s, &succnum ]( Node n, Label ) {
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
        assert( shared().ce.current_updated );
        assert( shared().ce.successor_id );
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
        int res = 0, i = 0;
        g().initials( [ this, &h, &s, &res, &i ]( Node, Node o, Label ) {
                ++i;
                Vertex v = s.fetch( o );
                res = s.equal( v.handle(), h ) ? i : res;
                this->g().release( o );
            } );
        return res;
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

        static visitor::ExpansionAction expansion( This &t, Vertex v ) {
            assert_eq( int( t.extension( v ).iteration() ), t.shared().iteration );
            return visitor::ExpansionAction::Expand;
        }

        static visitor::TransitionAction transition( This &t, Vertex from, Vertex to, Label ) {
            typename Store::template Guard< Extension > guard( from, to );
            if ( !t.store().valid( from ) ) {
                t.updateIteration( to );
                return visitor::TransitionAction::Expand;
            }
            if ( t.store().equal( to.handle(), t.shared().ce.initial ) ) {
                t.extension( to ).parent() = from.handle();
                assert_eq( int( t.extension( to ).iteration() ), t.shared().iteration );
                return visitor::TransitionAction::Terminate;
            }
            if ( t.updateIteration( to ) ) {
                t.extension( to ).parent() = from.handle();
                assert_eq( int( t.extension( from ).iteration() ), t.shared().iteration );
                return visitor::TransitionAction::Expand;
            }
            return visitor::TransitionAction::Forget;
        }
    };

    template< typename Algorithm >
    void _traceCycle( Algorithm &a ) {
        typedef FindCycle< typename Algorithm::Setup > Find;
        auto visitor = a.makeVisitor( *this, a, Find() );

        assert( a.store().valid( shared().ce.initial ) );
        if ( a.store().knows( shared().ce.initial ) ) {
            Vertex i = a.store().vertex( shared().ce.initial );
            visitor.queue( Vertex(), i.node(), Label() );
            i.disown();
        }
        visitor.processQueue();
    }

    // Obtaining CE output

    typedef std::vector< int > NumericTrace;
    typedef std::deque< Node > Trace;
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
    template< typename Alg, typename T, bool NT = true >
    NumericTrace generateTrace( Alg &a, G &_g, T trace, NumericTrace *numTrace = nullptr )
    {
        while ( !trace.empty() ) {
            if ( pool().valid( trace.front() ) ) {
                o_ce( a ) << _g.showNode( trace.front () ) << std::endl;
                // _g.release( trace.back() );
            } else {
                o_ce( a ) << "?" << std::endl;
            }
            trace.pop_front();
        }
        if ( NT )
            return numTrace ? *numTrace : numericTrace( a, _g, trace );
        return NumericTrace();
    }

    template< typename Alg >
    NumericTrace generateTrace( Alg &a, G &_g, Traces traces ) {
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
    void goal( Alg &a, T goal, bool saveTrail = true ) {
        o_ce( a ) << std::endl << "===== The goal =====" << std::endl << std::endl;
        std::deque< T > trace;
        trace.push_back( goal );

        if ( saveTrail )
            a.result().cycleTrail = generateTrace( a, g(), trace );
        else
            generateTrace< Alg, std::deque< T >, false >( a, g(), trace );
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

    template< typename TT, typename Domain, typename Alg >
    Traces parentTrace( Domain &d, Alg &a ) {
        Node parent = shared().ce.parent;
        std::deque< Handle > hTrace;
        std::set< uint64_t > hSeen;

        // trace backward to initial using handles
        shared().ce.current = shared().ce.initial;
        hTrace.push_front( shared().ce.initial );
        shared().ce.parent = Node();
        shared().ce.successor = Handle(); // !valid()
        shared().ce.successor_id = 0;
        shared().ce.is_ce_initial = false;
        bool first = false;

        while ( a.store().valid( shared().ce.current ) ) {
            // we need to make sure we check for initial even if size of trace = 1
            // that is even before we make fist step backward
            if ( TT::value == TraceType::Linear ) {
                shared().ce.successor_id = 0;
                shared().ce.current_updated = false;
                d.ring( &Alg::_ceIsInitial );
                if ( shared().ce.current_updated ) {
                    assert( shared().ce.successor_id );
                    break;
                }
            } else if ( TT::value == TraceType::Lasso ) {
                if ( shared().ce.is_ce_initial ) {
                    if ( first )
                        break;
                    else
                        first = true;
                }
            }
            else
                assert_unreachable( "invalid TraceType" );

            shared().ce.current_updated = false;
            d.ring( &Alg::_parentTrace );
            assert( shared().ce.current_updated );
            hTrace.push_front( shared().ce.current );

            if ( !shared().ce.is_ce_initial ) {
                assert( hSeen.count( shared().ce.current.asNumber() ) == 0 );
                hSeen.insert( shared().ce.current.asNumber() );
            }
        }
        hTrace.pop_front(); // initial

        return succTrace< TT >( d, a, parent, hTrace.begin(), hTrace.end() );
    }

    template< typename TT, typename Domain, typename Alg, typename Iter >
    Traces succTrace( Domain &d, Alg &a, Node parent, Iter hTraceBegin,
                      Iter hTraceEnd )
    {
        // track forward by handles, generating full traces
        Trace trace;
        NumericTrace numTrace;

        switch ( TT::value ) {
            case TraceType::Linear: {
                assert_neq( shared().ce.successor_id, 0 );
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
            assert( shared().ce.successor_id );
            trace.push_back( shared().ce.parent );
            numTrace.push_back( shared().ce.successor_id );
        }

        shared().ce.current = Handle();
        shared().ce.successor = Handle();

        return std::make_pair( trace, numTrace );
    }

    template< typename Alg, typename Iter, typename TT >
    Traces succTraceLocal( Alg &a, TT, Node parent,
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
        return generateLinear( a, g(), parentTrace< Linear >( d, a ) );
    }

    template< typename Domain, typename Alg >
    void lasso( Domain &d, Alg &a )
    {
        linear( d, a );
        ++ shared().iteration;
        d.parallel( &Alg::_traceCycle );
        generateLasso( a, g(), parentTrace< Lasso >( d, a ) );
    }
};

}
}

#endif
