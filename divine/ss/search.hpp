#pragma once

#include <memory>
#include <functional>
#include <future>
#include <vector>
#include <stack>

#include <brick-shmem>

/* tests */
#include <divine/ss/listen.hpp>
#include <divine/ss/fixed.hpp>
#include <divine/ss/random.hpp>

namespace divine {
namespace ss {

namespace shmem = ::brick::shmem;

struct Job
{
    virtual void start( int threads ) = 0;
    virtual void stop() = 0;
    virtual void wait() = 0;
    std::function< int64_t() > qsize;
};

enum class Order { PseudoBFS, DFS };

template< typename B, typename L >
struct Search : Job
{
    using Builder = B;
    using Listener = L;

    using State = typename Builder::State;
    using Label = typename Builder::Label;

    Builder _builder;
    Listener _listener;

    Order _order;
    int _thread_count;

    using WorkSet = std::pair< Builder *, Listener * >;
    using Vector = std::pair< std::mutex, std::vector< std::weak_ptr< WorkSet > > >;

    std::shared_ptr< Vector > _workset;
    std::vector< std::future< void > > _threads;
    std::shared_ptr< std::atomic< bool > > _terminate;
    struct Terminate {};

    using Worker = std::function< void() >;

    void order( Order o ) { _order = o; }

    Search( const B &b, const L &l )
        : _builder( b ), _listener( l ), _order( Order::PseudoBFS ),
          _workset( std::make_shared< Vector >() ),
          _terminate( new std::atomic< bool >( false ) )
    {}

    auto _register( Builder &b, Listener &l )
    {
        auto sp = std::make_shared< WorkSet >( &b, &l );
        std::lock_guard< std::mutex > _lock( _workset->first );
        _workset->second.push_back( sp );
        return sp;
    }

    template< typename Each >
    void ws_each( Each each )
    {
        ASSERT( _workset );
        std::lock_guard< std::mutex > _lock( _workset->first );
        for ( auto wptr : _workset->second )
            if ( auto ptr = wptr.lock() )
                each( *ptr->first, *ptr->second );
    }

    template< typename Push >
    void _initials( Listener &l, Builder &b, Push push )
    {
        b.initials(
            [&]( auto i )
            {
                auto b = l.state( i );
                if ( b == L::Process || b == L::AsNeeded )
                    push( i );
            } );
    }

    template< typename Act >
    void _state( Listener &l, State x, bool isnew, Act act )
    {
        auto b = l.state( x );
        if ( b == L::Terminate )
            _terminate->store( true ), throw Terminate();
        if ( b == L::Process || ( b == L::AsNeeded && isnew ) )
            act( isnew );
    }

    template< typename Succ >
    void _succs( Listener &l, Builder &b, State v, Succ succ )
    {
         b.edges(
             v, [&]( auto x, auto label, bool isnew )
             {
                 auto a = l.edge( v, x, label, isnew );
                 if ( a == L::Terminate )
                     _terminate->store( true ), throw Terminate();
                 if ( a == L::Process || ( a == L::AsNeeded && isnew ) )
                     succ( x, label, isnew );
            } );
    }

    Worker pseudoBFS()
    {
        shmem::SharedQueue< State > queue;
        shmem::StartDetector start;
        shmem::ApproximateCounter work;

        qsize = [=]() { return queue.chunkSize * queue.q->q.size(); };

        auto builder = _builder;
        auto listener = _listener;

        _initials( listener, builder,
                   [&]( auto st ) { queue.push( st ), ++ work; } );
        queue.flush();

        return [=]() mutable
        {
            auto _reg = _register( builder, listener );
            start.waitForAll( _thread_count );
            brick::types::Defer _( [&]() { _terminate->store( true ); } );

            try {
                while ( work && !_terminate->load() )
                {
                    if ( queue.empty() )
                    {
                        queue.flush();
                        work.sync();
                        continue;
                    }
                    auto v = queue.pop();
                    _succs( listener, builder, v,
                            [&]( auto s, auto, bool isnew )
                            {
                                _state( listener, s, isnew,
                                        [&]( bool ) { queue.push( s ), ++ work; } );
                            } );
                    -- work;
                }
            } catch ( Terminate ) {}

            ASSERT( _terminate->load() || queue.empty() );
        };
    }

    struct DFSItem
    {
        enum Type { Pre, Post } type;
        State state;
        Label label;

        DFSItem( Type t, State s, Label l = Label() )
            : type( t ), state( s ), label( l ) {}
    };

    Worker DFS()
    {
        auto builder = _builder;
        auto listener = _listener;

        return [=]() mutable
        {
            auto _reg = _register( builder, listener );
            std::stack< DFSItem > stack;
            builder.initials( [&]( auto st ) { stack.emplace( DFSItem::Pre, st ); } );

            while ( !stack.empty() && !_terminate->load() )
            {
                auto top = stack.top(); stack.pop();
                if ( top.type == DFSItem::Post )
                    listener.closed( top.state );
                else _state( listener, top.state, true,
                             [&]( bool )
                             {
                                 stack.emplace( DFSItem::Post, top.state );
                                 _succs( listener, builder, top.state,
                                         [&]( auto s, auto l, bool )
                                         {
                                             stack.emplace( DFSItem::Pre, s, l );
                                         } );
                             } );
            }
        };
    }

    Worker distributedBFS() { NOT_IMPLEMENTED(); }

    void start( int thread_count ) override
    {
        _thread_count = thread_count;
        Worker blueprint;

        switch ( _order )
        {
            case Order::PseudoBFS: blueprint = pseudoBFS(); break;
            case Order::DFS: blueprint = DFS(); break;
        }

        for ( int i = 0; i < _thread_count; ++i )
            _threads.emplace_back( std::async( blueprint ) );
    }

    void wait() override
    {
        auto cleanup = [&]
        {
            _terminate->store( true );
            for ( auto &res : _threads )
                if ( res.valid() )
                    /* not get(), since the only way futures are still valid
                     * here is because an exception is already propagating */
                    res.wait();
            ws_each( []( auto &, auto & ) { UNREACHABLE( "workset not empty!" ); } );
            _workset->second.clear();
        };

        while ( brick::shmem::wait( _threads.begin(), _threads.end(), cleanup ) !=
                std::future_status::ready );
    }

    void stop() override
    {
        _terminate->store( true );
    }
};

template< typename B, typename L >
auto make_search( B b, L l )
{
    return Search< B, L >( b, l );
}

template< typename B, typename L >
auto search( Order o, B b, int thr, L l )
{
    auto s = make_search( b, l );
    s.order( o );
    s.start( thr );
    return s.wait();
}

}

namespace t_ss {

#ifdef __divine__
static const int N = 4;
#else
static const int N = 1000;
#endif

struct Search
{
    void _fixed( ss::Order ord, int threads )
    {
        ss::Fixed builder{ { 1, 2 }, { 2, 3 }, { 1, 3 }, { 3, 4 } };
        int edgecount = 0, statecount = 0;
        ss::search(
            ord, builder, threads, ss::passive_listen(
                [&] ( auto f, auto t, auto )
                {
                    if ( f == 1 )
                        ASSERT( t == 2 || t == 3 );
                    if ( f == 2 )
                        ASSERT_EQ( t, 3 );
                    if ( f == 3 )
                        ASSERT_EQ( t, 4 );
                    ++ edgecount;
                },
                [&] ( auto ) { ++ statecount; } ) );
        ASSERT_EQ( edgecount, 4 );
        ASSERT_EQ( statecount, 4 );
    }

    void _random( ss::Order ord, int threads )
    {
        for ( unsigned seed = 0; seed < 10; ++ seed )
        {
            ss::Random builder{ 50, 120, seed };
            std::atomic< int > edgecount( 0 ), statecount( 0 );
            ss::search( ord, builder, threads, ss::passive_listen(
                            [&] ( auto, auto, auto ) { ++ edgecount; },
                            [&] ( auto ) { ++ statecount; } ) );
            ASSERT_EQ( statecount.load(), 50 );
            ASSERT_EQ( edgecount.load(), 120 );
        }
    }

    TEST( bfs_fixed ) { _fixed( ss::Order::PseudoBFS, 1 ); }
    TEST( bfs_random ) { _random( ss::Order::PseudoBFS, 1 ); }
    TEST( dfs_fixed ) { _fixed( ss::Order::DFS, 1 ); }
    TEST( dfs_random ) { _random( ss::Order::DFS, 1 ); }

    TEST( bfs_fixed_parallel )
    {
        _fixed( ss::Order::PseudoBFS, 2 );
        _fixed( ss::Order::PseudoBFS, 3 );
    }

    TEST( bfs_random_parallel )
    {
        _random( ss::Order::PseudoBFS, 2 );
        _random( ss::Order::PseudoBFS, 3 );
    }

    TEST( sequence )
    {
        std::vector< std::pair< int, int > > vec;
        for ( int i = 1; i <= N; ++i )
            vec.emplace_back( i, i + 1 );
        ss::Fixed builder( vec );
        int found = 0;
        ss::search(
            ss::Order::PseudoBFS, builder, 1, ss::passive_listen(
                [&] ( auto, auto, auto ) {}, [&]( auto ) { ++found; } ) );
        ASSERT_EQ( found, N + 1 );
    }

    TEST( navigate )
    {
        std::vector< std::pair< int, int > > vec;
        for ( int i = 1; i <= N; ++i )
        {
            vec.emplace_back( i, i + 1 );
            vec.emplace_back( i, i + N + 2 );
            vec.emplace_back( i + N + 2, i + 1 );
        }

        ss::Fixed builder( vec );
        int found = 0;
        ss::search(
            ss::Order::PseudoBFS, builder, 1, ss::listen(
                [&] ( auto f, auto t, auto )
                {
                    if ( f > 1000 )
                        return ss::Listen::Process;
                    if ( f % 2 == 1 )
                        return t > 1000 ? ss::Listen::Process : ss::Listen::Ignore;
                    else
                        return t < 1000 ? ss::Listen::Process : ss::Listen::Ignore;
                },
                [&]( auto ) { ++found; return ss::Listen::Process; } ) );
        ASSERT_EQ( found, int( 1.5 * N ) );
    }
};

}
}
