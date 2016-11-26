#pragma once

#include <memory>
#include <functional>
#include <future>
#include <vector>

#include <brick-shmem>

/* tests */
#include <divine/ss/listen.hpp>
#include <divine/ss/fixed.hpp>
#include <divine/ss/random.hpp>

namespace divine {
namespace ss {

namespace shmem = ::brick::shmem;

enum class Order { PseudoBFS, DFS };

template< typename B, typename L >
struct Search
{
    using Builder = B;
    using Listener = L;

    Builder _builder;
    Listener _listener;

    int _thread_count;
    Order _order;

    std::vector< std::future< void > > _threads;
    std::vector< Listener * > _listeners;
    std::shared_ptr< std::mutex > _listeners_lock;

    using Worker = std::function< void() >;

    void threads( int thr ) { _thread_count = thr; }
    void order( Order o ) { _order = o; }

    Search( const B &b, const L &l )
        : _builder( b ), _listener( l ), _listeners_lock( std::make_shared< std::mutex >() )
    {}

    void _started( Builder &, Listener &l )
    {
        std::lock_guard< std::mutex > lg( *_listeners_lock.get() );
        _listeners.push_back( &l );
    }

    Worker pseudoBFS()
    {
        shmem::SharedQueue< typename B::State > queue;
        shmem::StartDetector start;
        shmem::ApproximateCounter work;
        auto terminate = std::make_shared< std::atomic< bool > >( false );

        auto builder = _builder;
        auto listener = _listener;

        builder.initials(
            [&]( auto i )
            {
                auto b = listener.state( i );
                if ( b == L::Process || b == L::AsNeeded )
                    queue.push( i ), ++ work;
            } );
        queue.flush();

        return [=]() mutable
        {
            _started( builder, listener );
            start.waitForAll( _thread_count );
            brick::types::Defer _( [&]() { terminate->store( true ); } );

            while ( work && !terminate->load() )
            {
                if ( queue.empty() )
                {
                    queue.flush();
                    work.sync();
                    continue;
                }
                auto v = queue.pop();
                builder.edges(
                    v, [&]( auto x, auto label, bool isnew )
                    {
                        auto a = listener.edge( v, x, label, isnew );
                        if ( a == L::Terminate )
                            terminate->store( true );
                        if ( a == L::Process || ( a == L::AsNeeded && isnew ) )
                        {
                            auto b = listener.state( x );
                            if ( b == L::Terminate )
                                terminate->store( true );
                            if ( b == L::Process || ( b == L::AsNeeded && isnew ) )
                                queue.push( x ), ++ work;
                        }
                    } );
                -- work;
            }
            ASSERT( terminate->load() || queue.empty() );
        };
    }

    Worker DFS() { NOT_IMPLEMENTED(); }
    Worker distributedBFS() { NOT_IMPLEMENTED(); }

    void start()
    {
        Worker blueprint;

        switch ( _order )
        {
            case Order::PseudoBFS: blueprint = pseudoBFS(); break;
            case Order::DFS: blueprint = DFS(); break;
        }

        for ( int i = 0; i < _thread_count; ++i )
            _threads.emplace_back( std::async( blueprint ) );
    }

    void wait()
    {
        for ( auto &res : _threads )
            res.wait();
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
    s.threads( thr );
    s.order( o );
    s.start();
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
    void _bfs_fixed( int threads )
    {
        ss::Fixed builder{ { 1, 2 }, { 2, 3 }, { 1, 3 }, { 3, 4 } };
        int edgecount = 0, statecount = 0;
        ss::search(
            ss::Order::PseudoBFS, builder, threads, ss::passive_listen(
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

    void _bfs_random( int threads )
    {
        for ( unsigned seed = 0; seed < 10; ++ seed )
        {
            ss::Random builder{ 50, 120, seed };
            std::atomic< int > edgecount( 0 ), statecount( 0 );
            ss::search( ss::Order::PseudoBFS, builder, threads, ss::passive_listen(
                            [&] ( auto, auto, auto ) { ++ edgecount; },
                            [&] ( auto ) { ++ statecount; } ) );
            ASSERT_EQ( statecount.load(), 50 );
            ASSERT_EQ( edgecount.load(), 120 );
        }
    }

    TEST( bfs_fixed )
    {
        _bfs_fixed( 1 );
    }

    TEST( bfs_fixed_parallel )
    {
        _bfs_fixed( 2 );
        _bfs_fixed( 3 );
    }

    TEST( bfs_random )
    {
        _bfs_random( 1 );
    }

    TEST( bfs_random_parallel )
    {
        _bfs_random( 2 );
        _bfs_random( 3 );
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
