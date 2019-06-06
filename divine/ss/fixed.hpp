#pragma once

#include <brick-hashset>
#include <set>

namespace divine {
namespace ss {

using namespace brick;

struct Fixed
{
    using State = int;
    using Label = int;

    std::set< std::pair< int, int > > _edges;
    brq::concurrent_hash_set< int > _states;

    Fixed( std::initializer_list< std::pair< int, int > > il )
    {
        for ( auto e : il )
            _edges.insert( e );
    }

    Fixed( std::vector< std::pair< int, int > > il )
    {
        for ( auto e : il )
            _edges.insert( e );
    }

    template< typename Y >
    void edges( int from, Y yield )
    {
        for ( auto e : _edges )
            if ( e.first == from )
            {
                auto r = _states.insert( e.second );
                yield( e.second, 0, r.isnew() );
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
