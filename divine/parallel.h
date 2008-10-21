// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <wibble/sys/thread.h>
#include <divine/threading.h>
#include <divine/fifo.h>

namespace divine {

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
    T *m_master;
    std::vector< T > m_instances;
    std::vector< RunThread< T > > m_threads;

    int n;

    T &instance( int i ) { return m_instances[ i ]; }
    T &master() { return *m_master; }

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

    Parallel( T &master, int _n ) : m_master( &master ), n( _n )
    {
        m_instances.resize( n );
    }
};

template< typename T >
struct Domain {
    int m_id;
    std::map< T*, int > m_ids;

    TerminationConsole m_terminate;
    Parallel< T > *m_parallel;
    Parallel< T > &parallel() {
        if ( !m_parallel ) {
            // FIXME parametrize over number of workers
            m_parallel = new Parallel< T >( self(), 10 );
            for ( int i = 0; i < m_parallel->n; ++i )
                m_parallel->instance( i ).connect( self() );
        }
        return *m_parallel;
    }
    typedef divine::Fifo< int > Fifo;

    Fifo fifo;
    T *m_master;

    T &self() { return *static_cast< T* >( this ); }
    T &master() { return *m_master; }

    void connect( T &master ) {
        m_master = &master;
        m_id = master.obtainId( self() );
    }

    int peers() {
        if ( m_master )
            return master().m_id; // FIXME
        return 0;
    }

    int id() {
        return m_id;
    }

    int obtainId( T &t ) {
        if ( !m_ids.count( &t ) )
            m_ids[ &t ] = m_id ++;
        return m_ids[ &t ];
    }

    Fifo &queue( int i ) {
        return master().parallel().instance( i ).fifo;
    }

    Domain() : m_id( 0 ), m_parallel( 0 ), m_master( 0 ) {
    }
};

}
