#pragma once

#include <memory>
#include <functional>
#include <future>
#include <vector>

#include <brick-shmem.h>

#include <divine/statespace/perthread.hpp>

namespace divine {
namespace statespace {

template< typename L >
struct Aggregate
{
    using T = typename L::Aggregate;
};

template< typename L >
struct Result
{
    using T = typename L::Result;
};

template< typename L >
struct SearchInterface
{
    virtual typename Result< L >::T run( bool ) = 0;
};

template< typename B, typename L >
struct SearchBase : SearchInterface< L >
{
    B &_b;
    L &_l;
    SearchBase( B &builder, L &listener ) : _b( builder ), _l( listener ) {}
};

template< typename B, typename L >
struct DFS : SearchBase< B, L >
{
    struct Shared {};
    DFS( std::shared_ptr< Shared >, B &builder, L &listener ) : SearchBase< B, L >( builder, listener ) {}
    virtual typename Result< L >::T run( bool ) override
    {
        // ...
    }
};

template< typename B, typename L >
struct BFS : SearchBase< B, L >
{
    struct Shared
    {
        brick::shmem::SharedQueue< typename B::State > q;
        brick::shmem::StartDetector::Shared start;
    };

    std::shared_ptr< Shared > _sh;
    brick::shmem::StartDetector _start;

    virtual typename Result< L >::T run( bool initials ) override
    {
        auto q = _sh->q; /* make a copy */
        bool terminate = false;
        int count = 0;

        /* TODO the set of initial exploration states should be customisable */
        if ( initials )
            this->_b.initials( [&]( auto i )
                               {
                                   auto b = this->_l.state( i );
                                   if ( b == L::Process || b == L::AsNeeded )
                                       q.push( i );
                                   if ( count++ % 10 == 0 )
                                       q.flush();
                               } );

        while ( !terminate && !q.empty() )
        {
            auto v = q.pop();
            this->_b.edges(
                v, [&]( auto x, auto label, bool isnew )
                {
                    auto a = this->_l.edge( v, x, label );
                    if ( a == L::Terminate )
                        terminate = true;
                    if ( a == L::Process || ( a == L::AsNeeded && isnew ) )
                    {
                        auto b = this->_l.state( x );
                        if ( b == L::Terminate )
                            terminate = true;
                        if ( b == L::Process || ( b == L::AsNeeded && isnew ) )
                            q.push( x );
                    }
                } );
            if ( q.empty() )
                q.flush();
        }
        return typename Result< L >::T();
    }

    BFS( std::shared_ptr< Shared > sh, B &builder, L &listener )
        : SearchBase< B, L >( builder, listener ), _sh( sh ),
          _start( sh.start )
    {}
};

template< typename B, typename L >
struct DistributedBFS : BFS< B, L >
{
    using Shared = typename BFS< B, L >::Shared;
    virtual typename Result< L >::T run( bool ) override
    {
        ASSERT_UNIMPLEMENTED();
        /* also pull stuff out from networked queues with some probability */
        /* statically decide whether to push edges onto the local queue or
         * send it into the network */
    }

    DistributedBFS( std::shared_ptr< Shared > sh, B& builder, L &listener )
        : BFS< B, L >( sh, builder, listener )
    {}
};

enum class Order { PseudoBFS, DFS };

template< typename L >
using MakeSearch = std::function< std::unique_ptr< SearchInterface< L > >( int ) >;

template< template< typename, typename > class S, typename B, typename L >
MakeSearch< L > make( B &b, L &l )
{
    using B_PerThread = typename std::remove_reference< decltype( thread_access( b, 0 ) ) >::type;
    using Search = S< B_PerThread, L >;
    auto sh = std::make_shared< typename Search::Shared >();
    return [&b, &l, sh]( int i )
    {
        auto &builder = thread_access( b, i );
        return std::make_unique< Search >( sh, builder, l );
    };
}

/*
 * Parallel (and possibly distributed) graph search. The search is directed by
 * a Listener.
 */
template< typename B, typename Listen >
auto search( Order o, B &b, int threads, Listen l ) -> typename Aggregate< Listen >::T
{
    using R = typename Result< Listen >::T;
    using Agg = typename Aggregate< Listen >::T;

    MakeSearch< Listen > mkinstance;

    if ( /* g.peers().empty() && */ o == Order::PseudoBFS )
        mkinstance = make< BFS, B, Listen >( b, l );
    else if ( o == Order::PseudoBFS )
        mkinstance = make< DistributedBFS, B, Listen >( b, l );
    else if ( o == Order::DFS )
    {
        // ASSERT( g.peers().empty() );
        mkinstance = make< DFS, B, Listen >( b, l );
    }

    /* for ( auto p : g.peers() )
    {
        // TODO spawn remote threads attached to the respective peer stores
    } */

    std::vector< std::future< R > > rs;
    for ( int i = 0; i < threads; ++i )
        rs.emplace_back(
            std::async( [mkinstance, i]() {
                    auto s = mkinstance( i );
                    return s->run( i == 0 );
                } ) );

    Agg agg;
    for ( auto &r : rs )
        agg.add( r.get() );
    // TODO collect remote results
    return agg;
}

}
}

#include <divine/graph/fixed.hpp>
#include <divine/graph/random.hpp>

namespace divine_test {
namespace _statespace {

using namespace divine;

struct TestSearch
{
    TEST(bfs_fixed) {
        statespace::Fixed builder{ {1, 2}, {2, 3}, {1, 3}, {3, 4} };
        int edgecount = 0, statecount = 0;
        statespace::search(
            statespace::Order::PseudoBFS, builder, 1, statespace::passive_listen(
                [&] ( auto f, auto t, auto l ) { ++ edgecount; },
                [&] ( auto s ) { ++ statecount; } ) );
        ASSERT_EQ( edgecount, 4 );
        ASSERT_EQ( statecount, 4 );
    }

    TEST(bfs_random) {
        statespace::Random builder{ 50, 120 };
        int edgecount = 0, statecount = 0;
        statespace::search(
            statespace::Order::PseudoBFS, builder, 1, statespace::passive_listen(
                [&] ( auto f, auto t, auto l ) { ++ edgecount; },
                [&] ( auto s ) { ++ statecount; } ) );
        ASSERT_EQ( statecount, 50 );
        ASSERT_EQ( edgecount, 120 );
    }
};

}
}
