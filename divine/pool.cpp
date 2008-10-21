// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/pool.h>

namespace divine {

pthread_key_t ThreadPoolManager::s_pool_key;
pthread_once_t ThreadPoolManager::s_pool_once = PTHREAD_ONCE_INIT;
ThreadPoolManager::Available *ThreadPoolManager::s_available = 0;
wibble::sys::Mutex *ThreadPoolManager::s_mutex = 0;

Pool::Pool() {
    ThreadPoolManager::add( this );
}

Pool::Pool( const Pool& ) {
    ThreadPoolManager::add( this );
}

size_t Pool::peakAllocation() {
    size_t total = 0;
    Groups::iterator i;
    for ( i = m_groups.begin(); i != m_groups.end(); ++i ) {
        total += i->total;
    }
    return total;
}

size_t Pool::peakUsage() {
    size_t total = 0;
    Groups::iterator i;
    for ( i = m_groups.begin(); i != m_groups.end(); ++i ) {
        total += i->peak;
    }
    return total;
}

std::ostream &operator<<( std::ostream &o, const Pool &p )
{
    Pool::Groups::const_iterator i;
    for ( i = p.m_groups.begin(); i != p.m_groups.end(); ++i ) {
        if ( i->total == 0 )
            continue;
        o << "group " << i->item
          << " holds " << i->used
          << " (peaked " << i->peak
          << "), allocated " << i->allocated
          << " and freed " << i->freed << " bytes in "
          << i->blocks.size() << " blocks"
          << std::endl;
    }
    return o;
}

}
