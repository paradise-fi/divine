// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/pool.h>

using namespace wibble::sys;

namespace divine {

GlobalPools *GlobalPools::s_instance = 0;
pthread_once_t GlobalPools::s_once = PTHREAD_ONCE_INIT;

Pool::Pool() {
    GlobalPools::add( this );
}

Pool::Pool( const Pool& ) {
    GlobalPools::add( this );
}

Pool::~Pool() {
    GlobalPools::remove( this );
}

#ifndef DISABLE_POOLS
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
#endif

GlobalPools &GlobalPools::instance() {
    if ( s_instance )
        return *s_instance;
    pthread_once( &s_once, init_instance );
    assert( s_instance );
    return *s_instance;
}

void GlobalPools::init_instance() {
    s_instance = new GlobalPools();
    pthread_key_create(&s_instance->m_pool_key, pool_key_reclaim);
}

void GlobalPools::pool_key_reclaim( void *p )
{
    wibble::sys::MutexLock __l( mutex() );
    available().push_back( static_cast< Pool * >( p ) );
}

void GlobalPools::add( Pool *p ) {
    wibble::sys::MutexLock __l( mutex() );
    available().push_back( p );
}

Pool *GlobalPools::getSpecific() {
    return static_cast< Pool * >(
            pthread_getspecific( instance().m_pool_key ) );
}

void GlobalPools::setSpecific( Pool *p ) {
    pthread_setspecific( instance().m_pool_key, p );
}

void GlobalPools::remove( Pool *p ) {
    wibble::sys::MutexLock __l( mutex() );
    Available::iterator i =
        std::find( available().begin(), available().end(), p );
    if ( i != available().end() ) {
        available().erase( i );
    } else {
        assert( get() == p );
        setSpecific( 0 );
    }
}

Pool *GlobalPools::force( Pool *p ) {
    wibble::sys::MutexLock __l( mutex() );
    Pool *current = getSpecific();
    if ( current == p )
        return p;
    Available::iterator i =
        std::find( available().begin(), available().end(), p );
    assert( i != available().end() );
    Pool *p1 = *i, *p2 = getSpecific();
    available().erase( i );
    if ( p2 )
        available().push_back( p2 );
    setSpecific( p1 );
    return p1;
}

Pool *GlobalPools::get() {
    Pool *p = getSpecific();
    if ( !p ) {
        wibble::sys::MutexLock __l( mutex() );
        if ( available().empty() ) {
            new Pool();
        }
        // the ctor will register itself with available...
        assert( !available().empty() );
        p = available().front();
        available().pop_front();
        setSpecific( p );
    }
    return p;
}

}
