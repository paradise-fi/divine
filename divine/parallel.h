// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <wibble/sys/thread.h>
#include <divine/threading.h>
#include <divine/fifo.h>
#include <divine/blob.h>
#include <divine/barrier.h>

namespace divine {

/*
  A simple structure that runs a method of a class in a separate thread.
 */
template< typename T >
struct RunThread : wibble::sys::Thread {
    typedef void (T::*F)();
    T *t;
    F f;

    virtual void init() {}
    virtual void fini() {}

    void *main() {
        init();
        (t->*f)();
        fini();
        return 0;
    }

    RunThread( T &_t, F _f ) : t( &_t ), f( _f )
    {
    }
};

template< typename T, template< typename > class R = RunThread >
struct Parallel {
    T *m_master;
    std::vector< T > m_instances;
    std::vector< R< T > > m_threads;

    int n;

    T &instance( int i ) { return m_instances[ i ]; }
    T &master() { return *m_master; }

    typename T::Shared &shared( int i ) { return instance( i ).shared; }

    R< T > &thread( int i ) { return m_threads[ i ]; }

    template< typename F >
    void initThreads( F f ) {
        m_threads.clear();
        for ( int i = 0; i < n; ++i ) {
            instance( i ).shared = master().shared;
            m_threads.push_back( R< T >( instance( i ), f ) );
        }
    }

    void runThreads() {
        for ( int i = 0; i < n; ++i )
            thread( i ).start();
        for ( int i = 0; i < n; ++i )
            thread( i ).join();
    }

    template< typename F >
    void run( F f ) {
        initThreads( f );
        runThreads();
    }

    Parallel( T &master, int _n ) : m_master( &master ), n( _n )
    {
        m_instances.resize( n );
    }
};

template< typename T >
struct BarrierThread : RunThread< T > {
    Barrier< T > *m_barrier;

    void setBarrier( Barrier< T > &b ) {
        m_barrier = &b;
    }

    virtual void init() {
        assert( m_barrier );
        m_barrier->started( this->t );
    }

    virtual void fini() {
        // m_done is true if termination has been done, in which case all of
        // the mutexes are unlocked. (and unlocking an already unlocked mutex
        // locks it... d'OH)
        m_barrier->done( this->t );
    }

    BarrierThread( T &_t, typename RunThread< T >::F _f )
        : RunThread< T >( _t, _f ), m_barrier( 0 )
    {
    }
};

template< typename T >
struct Domain {
    struct Parallel : divine::Parallel< T, BarrierThread >
    {
        Domain< T > *m_domain;

        template< typename F >
        void run( F f ) {
            initThreads( f );
            for ( int i = 0; i < this->n; ++i )
                this->thread( i ).setBarrier( m_domain->m_barrier );
            this->runThreads();
            m_domain->m_barrier.clear();
        }

        Parallel( Domain *dom, T &master, int _n )
            : divine::Parallel< T, BarrierThread >( master, _n ),
              m_domain( dom )
        {
        }
    };

    typedef divine::Fifo< Blob > Fifo;

    int m_id;
    std::map< T*, int > m_ids;

    Fifo fifo;

    Barrier< T > m_barrier;
    Parallel *m_parallel;
    T *m_master;

    int n;

    Parallel &parallel() {
        if ( !m_parallel ) {
            m_parallel = new Parallel( this, self(), n );
            m_barrier.setExpect( n );
            for ( int i = 0; i < m_parallel->n; ++i ) {
                m_parallel->instance( i ).connect( self() );
            }
        }
        return *m_parallel;
    }

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

    bool isIdle() {
        return this->fifo.empty();
    }

    Fifo &queue( int i ) {
        return master().parallel().instance( i ).fifo;
    }

    Domain( int _n = 4 ) : m_id( 0 ), m_parallel( 0 ), m_master( 0 ), n( _n ) {
    }
};

}
