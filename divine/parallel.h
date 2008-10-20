// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <wibble/sys/thread.h>

namespace divine {

template< typename T >
struct PageAlign {
    T t;
    char __align[4096 - sizeof(T)];
};

/*
  A simple structure that runs a method of a class in a separate thread.
 */
template< typename T >
struct RunThread : wibble::sys::Thread {
    typedef void (T::*F)();
    T *t;
    F f;

    void *main() {
        (t->*f)();
        return 0;
    }

    RunThread( T &_t, F _f ) : t( &_t ), f( _f )
    {
    }
};

template< typename T >
struct Parallel {
    T &m_master;
    std::vector< PageAlign< T > > m_instances;
    std::vector< RunThread< T > > m_threads;

    int n;

    T &instance( int i ) { return m_instances[ i ].t; }
    T &master() { return m_master; }
    typename T::Shared &shared( int i ) { return instance( i ).shared; }
    RunThread< T > &thread( int i ) { return m_threads[ i ]; }

    template< typename F >
    void run( F f ) {
        m_threads.clear();
        for ( int i = 0; i < n; ++i ) {
            instance( i ).shared = master().shared;
            m_threads.push_back( RunThread< T >( instance( i ), f ) );
        }
        for ( int i = 0; i < n; ++i )
            thread( i ).start();
        for ( int i = 0; i < n; ++i )
            thread( i ).join();
    }

    Parallel( T &master, int _n ) : m_master( master ), n( _n )
    {
        m_instances.resize( n );
    }
};

}
