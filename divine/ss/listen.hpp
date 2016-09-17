namespace divine {
namespace ss {

struct TrivialGroup
{
    TrivialGroup operator+( const TrivialGroup & ) { return *this; }
};

struct Listen
{
    enum Action { Process, Ignore, AsNeeded, Terminate };

    /* a new edge has been discovered */
    template< typename State, typename Label >
    Action edge( State, State, Label, bool ) { return AsNeeded; }

    /* edge() returned Process on a state */
    template< typename State >
    Action state( State ) { return AsNeeded; }

    /* a state is about to be merged into the store */
    template< typename State >
    Action merge( State ) { return Process; }

    template< typename State >
    Action closed( State ) { return Process; }
};

template< typename E, typename S, typename M, typename C >
struct GenericListen : Listen
{
    E _e; S _s; M _m; C _c;
    TrivialGroup result;

    GenericListen( E e, S s, M m, C c )
        : _e( e ), _s( s ), _m( m ), _c( c )
    {}

    template< typename State, typename Label, typename _E = E >
    auto edge( State f, State t, Label l, bool n ) ->
        decltype( std::declval< _E >()( f, t, l, n ) )
    { return _e( f, t, l, n ); }

    template< typename State, typename Label, typename _E = E >
    auto edge( State f, State t, Label l, bool n ) ->
        decltype( std::declval< _E >()( f, t, l ) )
    { return _e( f, t, l ); }

    template< typename State >
    Action state( State s ) { return _s( s ); }

    template< typename State >
    Action merge( State s ) { return _m( s ); }

    template< typename State >
    Action closed( State s ) { return _c( s ); }
};

template< typename E, typename S, typename M, typename C >
struct PassiveListen : Listen
{
    E _e; S _s; M _m; C _c;
    TrivialGroup result;

    PassiveListen( E e, S s, M m, C c )
        : _e( e ), _s( s ), _m( m ), _c( c )
    {}

    template< typename State, typename Label, typename _E = E >
    auto edge( State f, State t, Label l, bool n ) ->
        decltype( std::declval< _E >()( f, t, l, n ), Action() )
    {
        _e( f, t, l, n );
        return Listen::AsNeeded;
    }

    template< typename State, typename Label, typename _E = E >
    auto edge( State f, State t, Label l, bool n ) ->
        decltype( std::declval< _E >()( f, t, l ), Action() )
    {
        _e( f, t, l );
        return Listen::AsNeeded;
    }

    template< typename State >
    Action state( State s )
    {
        _s( s );
        return Listen::AsNeeded;
    }

    template< typename State >
    Action merge( State s )
    {
        _m( s );
        return Listen::Process;
    }

    template< typename State >
    Action closed( State s )
    {
        _c( s );
        return Listen::Process;
    }
};

namespace {
auto _edge_as_needed = []( auto, auto, auto ) { return Listen::AsNeeded; };
auto _state_as_needed = []( auto ) { return Listen::AsNeeded; };
auto _state_process = []( auto ) { return Listen::Process; };

auto _edge_noop = []( auto, auto, auto ) {};
auto _state_noop = []( auto, auto, auto ) {};
}

template< typename E = decltype( _edge_as_needed ),
          typename S = decltype( _state_as_needed ),
          typename M = decltype( _state_process ),
          typename C = decltype( _state_process ) >
GenericListen< E, S, M, C > listen( E e = _edge_as_needed, S s = _state_as_needed,
                                    M m = _state_process, C c = _state_process )
{
    return GenericListen< E, S, M, C >( e, s, m, c );
}

template< typename E = decltype( _edge_noop ),
          typename S = decltype( _state_noop ),
          typename M = decltype( _state_noop ),
          typename C = decltype( _state_noop ) >
PassiveListen< E, S, M, C > passive_listen( E e = _edge_noop, S s = _state_noop,
                                            M m = _state_noop, C c = _state_noop )
{
    return PassiveListen< E, S, M, C >( e, s, m, c );
}

}
}
