#pragma once

#include <brick-shmem>
#include <set>

namespace divine {
namespace statespace {

struct Fixed
{
    using State = int;
    using Lock = brick::shmem::SpinLock;

    std::set< std::pair< int, int > > _edges;
    std::set< int > _states;
    Lock _lock;

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
                std::unique_lock< Lock > _g( _lock );
                auto r = _states.insert( e.second );
                _g.unlock();
                yield( e.second, 0, r.second );
            }
    }

    template< typename Y >
    void initials( Y yield )
    {
        yield( 1 );
        std::lock_guard< Lock > _g( _lock );
        _states.insert( 1 );
    }
};

}
}
