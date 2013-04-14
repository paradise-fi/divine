// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <stdint.h>
#include <sstream>
#include <utility>

#ifndef DIVINE_LTLCE_H
#define DIVINE_LTLCE_H

namespace divine {
namespace algorithm {

template< typename Node, typename VertexId >
struct CeShared {
    VertexId initial;
    VertexId current;
    VertexId successor;
    bool current_updated;
    bool is_ce_initial;
    int successor_id;
    Node parent;
};

template< typename BS, typename Node, typename VertexId >
typename BS::bitstream &operator<<( BS &bs, const CeShared< Node, VertexId > &sh )
{
    return bs << sh.initial << sh.current << sh.successor << sh.current_updated
              << sh.successor_id << sh.parent;
}

template< typename BS, typename Node, typename VertexId >
typename BS::bitstream &operator>>( BS &bs, CeShared< Node, VertexId > &sh )
{
    return bs >> sh.initial >> sh.current >> sh.successor >> sh.current_updated
              >> sh.successor_id >> sh.parent;
}

template< typename _Setup, typename Shared, typename Extension, typename Hasher >
struct LtlCE {
    typedef typename _Setup::Graph G;
    typedef typename G::Node Node;
    typedef typename G::Label Label;
    typedef typename _Setup::Vertex Vertex;
    typedef typename _Setup::VertexId VertexId;
    typedef LtlCE< _Setup, Shared, Extension, Hasher > This;
    typedef CeShared< Node, VertexId > ThisCeShared;

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

    hash_t hash( Node n ) {
        assert( _hasher );
        return _hasher->hash( n );
    }

    Pool& pool() {
        return g().base().alloc.pool();
    }

    Extension &extension( Node n ) {
        return pool().template get< Extension >( n );
    }

    Extension& extension( VertexId vi ) {
        return vi.template extension< Extension >( pool() );
    }

    bool updateIteration( Vertex t ) {
        int old = extension( t.getNode() ).iteration;
        extension( t.getNode() ).iteration = shared().iteration;
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
        if ( s.owner( w, shared().ce.current ) == w.id() ) {
            VertexId h = s.fetchVertexId( shared().ce.current );
            shared().ce.successor = h;
            shared().ce.current = extension( h ).parent;
            shared().ce.current_updated = true;
            VertexId init = shared().ce.initial;
            shared().ce.is_ce_initial = visitor::equalId( s, init, h );
        }
    }

    template < typename Worker, typename Store >
    void _ceIsInitial( Worker& w, Store& s ) {
        if ( shared().ce.current_updated )
            return;
        if ( !s.valid( shared().ce.current ) )
            return;
        if ( s.owner( w, shared().ce.current ) == w.id() ) {
            shared().ce.successor_id = whichInitial( shared().ce.current, s );
            shared().ce.current_updated = shared().ce.successor_id > 0;
        }
    }

    template < typename Worker, typename Store >
    void _successorTrace( Worker& w, Store& s ) {
        if ( shared().ce.current_updated )
            return;
        assert( s.valid( shared().ce.parent ) );
        if ( s.owner( w, shared().ce.current ) == w.id() ) {
            int id = 0;
            Node parent = shared().ce.parent;
            g().allSuccessors( parent,
                    [ this, &s, &id ]( Node n, Label ) {
                        if ( this->shared().ce.current_updated )
                            return;
                        ++id;
                        Vertex sSt = std::get< 0 >( s.fetch( n, s.hash( n ) ) );
                        VertexId h = sSt.getVertexId();
                        n = sSt.getNode();
                        if ( visitor::equalId( s, h, this->shared().ce.current ) ) {
                            this->shared().ce.parent = n;
                            this->shared().ce.successor_id = id;
                            this->shared().ce.current_updated = true;
                        } else {
                            this->g().release( n );
                        }
                    } );
            shared().ce.current_updated = true;
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

    template < typename Store >
    int whichInitial( VertexId handle, Store& s ) {
        int res = 0, i = 0;
        VertexId h = handle;
        g().initials( [ this, &h, &s, &res, &i ]( Node, Node o, Label ) {
                ++i;
                VertexId ho = std::get< 0 >( s.fetch( o, s.hash( o ) ) ).getVertexId();
                res = visitor::equalId( s, h, ho ) ? i : res;
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

        static visitor::ExpansionAction expansion( This &, Vertex n ) {
            return visitor::ExpandState;
        }

        static visitor::TransitionAction transition( This &t, Vertex from, Vertex to, Label ) {
            if ( !from.getNode().valid() )
                return visitor::ExpandTransition;
            if ( from.getNode().valid() && t.whichInitial( to.getNode() ) ) {
                t.extension( to.getNode() ).parent = from.getVertexId();
                return visitor::TerminateOnTransition;
            }
            if ( t.updateIteration( to ) ) {
                t.extension( to.getNode() ).parent = from.getVertexId();
                return visitor::ExpandTransition;
            }
            return visitor::ForgetTransition;
        }
    };

    template< typename Algorithm >
    void _traceCycle( Algorithm &a ) {
        typedef FindCycle< typename Algorithm::Setup > Find;
        typename Find::Visitor::template Implementation< Find, Algorithm >
            visitor( *this, a, a.graph(), a.store(), a.data );

        assert( shared().ce.parent.valid() );
        if ( a.store().owner( a, shared().ce.parent ) == a.id() ) {
            assert( a.store().owner( a, shared().ce.initial ) == a.id() );
            Vertex par = std::get< 0 >( a.store().fetch( shared().ce.parent,
                              a.store().hash( shared().ce.parent ) ) );
            shared().ce.parent = par.getNode();
            assert( visitor::equalId( a.store(), par.getVertexId(),
                    shared().ce.initial ) ); // since initial must be parent's handle
            visitor.queue( Vertex(), par.getNode(), Label() );
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
    struct Linear { static constexpr TraceType type = TraceType::Linear; };
    struct Lasso { static constexpr TraceType type = TraceType::Lasso; };

    template< typename Domain, typename Alg, typename TT >
    Traces parentTrace( Domain &d, Alg &a, TT traceType ) {
        Node parent = shared().ce.parent;
        std::deque< VertexId > hTrace;

        // trace backward to initial using handles
        shared().ce.current = shared().ce.initial;
        hTrace.push_front( shared().ce.initial );
        shared().ce.parent = Node();
        shared().ce.successor = VertexId(); // !valid()
        shared().ce.successor_id = 0;
        bool first = false;
        while ( a.store().valid( shared().ce.current ) ) {
            shared().ce.current_updated = shared().ce.is_ce_initial = false;
            d.ring( &Alg::_parentTrace );
            assert( shared().ce.current_updated );
            hTrace.push_front( shared().ce.current );

            if ( traceType.type == TraceType::Linear ) {
                shared().ce.successor_id = 0;
                shared().ce.current_updated = false;
                d.ring( &Alg::_ceIsInitial );
                if ( shared().ce.current_updated )
                    break;
            } else if ( traceType.type == TraceType::Lasso ) {
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

        return succTrace( d, a, traceType, parent, hTrace.begin(), hTrace.end() );
    }


    template< typename Domain, typename Alg, typename Iter, typename TT >
    Traces succTrace( Domain &d, Alg &a, TT traceType, Node parent,
            Iter hTraceBegin, Iter hTraceEnd )
    {
        // track forward by handles, generating full traces
        Trace trace;
        NumericTrace numTrace;

        switch ( traceType.type ) {
            case TraceType::Linear: {
                if ( shared().ce.successor_id == 0 ) // empty CE
                    return std::make_pair( trace, numTrace );
                Node initial = getInitialById( shared().ce.successor_id );
                initial = std::get< 0 >( a.store().fetch( initial,
                            a.store().hash( initial ) ) ).getNode();
                visitor::setPermanent( a.pool(), initial );
                assert( initial.valid() );
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
        shared().ce.successor = VertexId();
        while ( hTraceBegin != hTraceEnd ) {
            shared().ce.current_updated = false;
            shared().ce.successor_id = 0;
            shared().ce.current = *hTraceBegin;
            ++hTraceBegin;
            d.ring( &Alg::_successorTrace );
            assert( shared().ce.current_updated );
            visitor::setPermanent( a.pool(), shared().ce.parent );
            trace.push_back( shared().ce.parent );
            numTrace.push_back( shared().ce.successor_id );
        }

        shared().ce.current = VertexId();
        shared().ce.successor = VertexId();

        return std::make_pair( trace, numTrace );
    }

    template< typename Alg, typename Iter, typename TT >
    Traces succTraceLocal( Alg &a, TT traceType, Node parent,
            Iter hTraceBegin, Iter hTraceEnd )
    {
        // track forward by handles, generating full traces
        Trace trace;
        NumericTrace numTrace;

        switch ( traceType.type ) {
            case TraceType::Linear: {
                if ( hTraceBegin == hTraceEnd ) // empty CE
                    return std::make_pair( trace, numTrace );
                int i = 0;
                bool done = false;
                a.graph().initials( [&]( Node, Node o, Label ) {
                        if ( !done ) {
                            ++i;
                            auto vi = std::get< 0 >( a.store()
                                .fetch( o, a.store().hash( o ) ) )
                                .getVertexId();
                            if ( visitor::equalId( a.store(), vi, *hTraceBegin ) ) {
                                parent = o;
                                done = true;
                            }
                        }
                    } );
                assert( parent.valid() );
                visitor::setPermanent( a.pool(), parent );
                trace.push_back( parent );
                numTrace.push_back( i );
                break; }
            case TraceType::Lasso:
                break;
            default:
                assert_die();
        }


        assert( a.store().valid( parent ) );
        Node successor;
        for ( ++hTraceBegin; hTraceBegin != hTraceEnd; ++hTraceBegin ) {
            int i = 0;
            bool done = false;
            a.graph().allSuccessors( parent, [&]( Node t, Label ) {
                    if ( !done ) {
                        ++i;
                        auto vi = std::get< 0 >( a.store()
                            .fetch( t, a.store().hash( t ) ) ).getVertexId();
                        if ( visitor::equalId( a.store(), vi, *hTraceBegin ) ) {
                            parent = t;
                            done = true;
                        }
                    }
                } );
            assert( done );
            visitor::setPermanent( a.pool(), parent );
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
