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

}
