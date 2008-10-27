// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/pool.h>

using namespace wibble::sys;

namespace divine {

volatile bool ThreadPoolManager::s_init_done = false;
pthread_key_t ThreadPoolManager::s_pool_key;
ThreadPoolManager::Available *ThreadPoolManager::s_available = 0;
wibble::sys::Mutex *ThreadPoolManager::s_mutex = 0;

Pool::Pool() {
    ThreadPoolManager::add( this );
}

Pool::Pool( const Pool& ) {
    ThreadPoolManager::add( this );
}

Pool::~Pool() {
    ThreadPoolManager::remove( this );
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

void ThreadPoolManager::init() {
    if ( s_init_done )
        return;
    MutexLock __l( mutex() );
    if ( s_init_done )
        return;
    s_init_done = true;
    pthread_key_create(&s_pool_key, pool_key_reclaim);
}

void ThreadPoolManager::add( Pool *p ) {
    init();
    wibble::sys::MutexLock __l( mutex() );
    available().push_back( p );
}

void ThreadPoolManager::remove( Pool *p ) {
    init();
    wibble::sys::MutexLock __l( mutex() );
    Available::iterator i =
        std::find( available().begin(), available().end(), p );
    if ( i != available().end() ) {
        available().erase( i );
    } else {
        assert( get() == p );
        pthread_setspecific( s_pool_key, 0 );
    }
}

Pool *ThreadPoolManager::force( Pool *p ) {
    init();
    wibble::sys::MutexLock __l( mutex() );
    Pool *current =
        static_cast< Pool * >( pthread_getspecific( s_pool_key ) );
    if ( current == p )
        return p;
    Available::iterator i =
        std::find( available().begin(), available().end(), p );
    assert( i != available().end() );
    Pool *p1 = *i, *p2 =
               static_cast< Pool * >( pthread_getspecific( s_pool_key ) );
    available().erase( i );
    if ( p2 )
        available().push_back( p2 );
    pthread_setspecific( s_pool_key, p1 );
    return p1;
}

Pool *ThreadPoolManager::get() {
    init();
    Pool *p = static_cast< Pool * >( pthread_getspecific( s_pool_key ) );
    if ( !p ) {
        wibble::sys::MutexLock __l( mutex() );
        if ( available().empty() ) {
            new Pool();
        }
        // the ctor will register itself with available...
        assert( !available().empty() );
        p = available().front();
        available().pop_front();
        pthread_setspecific( s_pool_key, p );
    }
    return p;
}

}
