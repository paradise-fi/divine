#pragma once
#include <set>

namespace divine {
namespace statespace {

struct Fixed
{
    using State = int;

    std::set< std::pair< int, int > > _edges;
    std::set< int > _states;

    Fixed( std::initializer_list< std::pair< int, int > > il )
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
                yield( e.second, 0, !_states.count( e.second ) );
                _states.insert( e.second );
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
