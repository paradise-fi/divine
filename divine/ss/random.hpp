#pragma once

#include <brick-shmem>
#include <set>
#include <random>

namespace divine {
namespace statespace {

struct Random
{
    using Lock = brick::shmem::SpinLock;
    using State = int;

    std::vector< std::vector< int > > _succs;
    std::set< int > _states;
    Lock _lock;

    Random( int vertices, int edges, unsigned seed = 0 )
    {
        std::mt19937 rand{ seed };
        std::uniform_int_distribution< int > dist( 0, vertices - 1 );
        _succs.resize( vertices );
        std::set< int > connected;
        int last = 0;

        /* first connect everything */
        while ( connected.size() < vertices ) {
            int next = dist( rand );
            if ( connected.count( next ) )
                continue;
            _succs[ last ].push_back( next );
            connected.insert( next );
            last = next;
            -- edges;
        }

        /* add more edges at random */
        while ( edges > 0 ) {
        next:
            int from = dist( rand );
            int to = dist( rand );
            for ( auto c : _succs[ from ] )
                if ( to == c )
                    goto next;
            _succs[ from ].push_back( to );
            -- edges;
        }
    }

    template< typename Y >
    void edges( int from, Y yield )
    {
        for ( auto t : _succs[ from ] )
        {
            std::unique_lock< Lock > _g( _lock );
            auto r = _states.insert( t );
            _g.unlock();
            yield( t, 0, r.second );
        }
    }

    template< typename Y >
    void initials( Y yield )
    {
        yield( 0 );
        std::lock_guard< Lock > _g( _lock );
        _states.insert( 0 );
    }
};

}
}
