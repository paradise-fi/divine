#pragma once

#include <brick-hashset>
#include <set>
#include <random>

namespace divine {
namespace ss {

struct Random
{
    using State = int;
    using Label = int;

    std::vector< std::vector< int > > _succs;
    brq::concurrent_hash_set< int > _states;

    Random( int vertices, int edges, unsigned seed = 0 )
    {
        std::mt19937 rand{ seed };
        std::uniform_int_distribution< int > dist( 1, vertices );
        _succs.resize( vertices + 1 );
        std::set< int > connected;
        int last = 1;

        /* first connect everything */
        while ( int( connected.size() ) < vertices ) {
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
            auto r = _states.insert( t );
            yield( t, 0, r.isnew() );
        }
    }

    template< typename Y >
    void initials( Y yield )
    {
        yield( 1 );
        _states.insert( 1 );
    }
};

}
}
