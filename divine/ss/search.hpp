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

template< typename L >
struct SearchInterface
{
    virtual void run( int, bool ) = 0;
    virtual ~SearchInterface() {}
};

template< typename B, typename L >
struct SearchBase : SearchInterface< L >
{
    B _b;
    L _l;
    SearchBase( const B &b, const L &l ) : _b( b ), _l( l ) {}
    auto result() { return _l.result; }
};

template< typename B, typename L >
struct DFS : SearchBase< B, L >
{
    virtual void run( int, bool ) override
    {
        NOT_IMPLEMENTED();
    }

    DFS( const B &b, const L &l ) : SearchBase< B, L >( b, l ) {}
};

template< typename B, typename L >
struct BFS : SearchBase< B, L >
{
    shmem::SharedQueue< typename B::State > _q;
    shmem::StartDetector _start;
    shmem::ApproximateCounter _work;
    std::shared_ptr< std::atomic< bool > > _terminate;

    BFS( const B &b, const L &l )
        : SearchBase< B, L >( b, l ),
          _terminate( new std::atomic< bool >( false ) )
    {}

    virtual void run( int peers, bool initials ) override
    {
        int count = 0;

        if ( initials )
        {
            this->_b.initials( [&]( auto i )
                               {
                                   auto b = this->_l.state( i );
                                   if ( b == L::Process || b == L::AsNeeded )
                                       _q.push( i ), ++ _work;
                               } );
            _q.flush();
        }

        _start.waitForAll( peers );

        while ( _work && !_terminate->load() )
        {
            if ( _q.empty() )
            {
                _q.flush();
                _work.sync();
                continue;
            }
            auto v = _q.pop();
            this->_b.edges(
                v, [&]( auto x, auto label, bool isnew )
                {
                    auto a = this->_l.edge( v, x, label );
                    if ( a == L::Terminate )
                        _terminate->store( true );
                    if ( a == L::Process || ( a == L::AsNeeded && isnew ) )
                    {
                        auto b = this->_l.state( x );
                        if ( b == L::Terminate )
                            _terminate->store( true );
                        if ( b == L::Process || ( b == L::AsNeeded && isnew ) )
                            _q.push( x ), ++ _work;
                    }
                } );
            -- _work;
        }
        ASSERT( _terminate->load() || _q.empty() );
    }
};

template< typename B, typename L >
struct DistributedBFS : BFS< B, L >
{
    virtual void run( int, bool ) override
    {
        NOT_IMPLEMENTED();
        /* also pull stuff out from networked queues with some probability */
        /* decide whether to push edges onto the local queue or send it into
         * the network */
    }

    DistributedBFS( const B &b, const L &l ) : BFS< B, L >( b, l ) {}
};

enum class Order { PseudoBFS, DFS };

template< template< typename, typename > class S, typename B, typename L >
auto do_search( const B &b, const L &l, int threads )
{
    /* for ( auto p : g.peers() )
    {
        // TODO spawn remote threads attached to the respective peer stores
    } */

    std::vector< std::future< decltype( l.result ) > > ls;

    S< B, L > search( b, l );
    for ( int i = 0; i < threads; ++i )
        ls.emplace_back(
            std::async( [&search, i, threads]()
                        {
                            S< B, L > s( search );
                            s.run( threads, i == 0 );
                            return s.result();
                        } ) );

    decltype( l.result ) result = l.result;
    for ( auto &l : ls )
        result = result + l.get();
    // TODO: remote results

    return result;
}

/*
 * Parallel (and possibly distributed) graph search. The search is directed by
 * a Listener.
 */
template< typename B, typename L >
auto search( Order o, B b, int threads, L l )
{
    if ( threads == 1 && o == Order::PseudoBFS )
    {
        BFS< B, L > search( b, l );
        search.run( 1, true );
        return search.result();
    }
    else if ( /* g.peers().empty() && */ o == Order::PseudoBFS )
        return do_search< BFS >( b, l, threads );
    else if ( o == Order::PseudoBFS )
        return do_search< DistributedBFS >( b, l, threads );
    else if ( o == Order::DFS )
    {
        // ASSERT( g.peers().empty() );
        return do_search< DFS >( b, l, threads );
    }
    UNREACHABLE( "don't know how to run a search" );
}

}

namespace t_ss {

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
};

}
}
