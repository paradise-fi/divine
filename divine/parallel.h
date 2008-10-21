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
struct Barrier {
    typedef wibble::sys::Mutex Mutex;
    typedef wibble::sys::MutexLock MutexLock;

    typedef std::map< T *, Mutex > Mutexes;
    Mutexes m_busy;
    Mutex m_terminating;
    std::map< T *, wibble::sys::Condition > m_condition;
    volatile bool m_done;

    int m_regd, m_expect;

    Mutex &mutex( T *t ) {
        return m_busy[ t ];
    }

    Condition &condition( T *t ) {
        return m_condition[ t ];
    }

    void unlockSet( std::set< Mutex * > &locks )
    {
        for ( std::set< Mutex * >::iterator j = locks.begin();
              j != locks.end(); ++j )
            (*j)->unlock();
    }

    bool idle( T *who ) {
        if ( m_regd < m_expect )
            return false;

        MutexLock __l( m_terminating );
        typename Mutexes::iterator i;
        std::set< Mutex * > locked;

        m_done = true;

        for ( i = m_busy.begin(); i != m_busy.end(); ++i ) {
            if ( i->first != who && !i->second.trylock() ) {
                m_done = false;
            } else {
                locked.insert( &(i->second) );
            }
        }

        if ( m_done ) { // we have locked all mutexes, check for work
            for ( i = m_busy.begin(); i != m_busy.end(); ++i ) {
                if ( !i->first->isIdle() ) {
                    m_done = false;
                    condition( i->first ).signal(); // wake up that thread
                }
            }
        }

        unlockSet( locked );

        if ( !m_done ) {
            __l.drop(); // we are safe now
            condition( who ).wait( mutex( who ) );
        } else {
            // We win. Wake up everyone so they can get through the barrier.
            for ( i = m_busy.begin();  i != m_busy.end(); ++i ) {
                if ( i-> first == who )
                    continue;
                condition( i->first ).signal();
            }
        }
        return m_done;
    }

    void connect( T *t ) {
        m_busy[ t ] = Mutex();
        m_condition[ t ] = wibble::sys::Condition();
    }

    void started( T *t ) {
        MutexLock __l( m_terminating );
        mutex( t ).lock();
        ++ m_regd;
    }

    void clear() {
        m_regd = 0;
        m_expect = 0;
        m_busy.clear();
        m_condition.clear();
    }

    void setExpect( int n ) {
        m_regd = 0;
        m_expect = n;
    }
};

template< typename T >
struct BarrierThread : RunThread< T > {
    Barrier< T > *m_barrier;

    void setBarrier( Barrier< T > &b ) {
        m_barrier = &b;
        b.connect( this->t );
    }

    virtual void init() {
        assert( m_barrier );
        m_barrier->started( this->t );
    }

    virtual void fini() {
        // m_done is true if termination has been done, in which case all of
        // the mutexes are unlocked. (and unlocking an already unlocked mutex
        // locks it... d'OH)
        if ( !m_barrier->m_done )
            m_barrier->mutex( this->t ).unlock();
    }

    BarrierThread( T &_t, typename RunThread< T >::F _f )
        : RunThread< T >( _t, _f ), m_barrier( 0 )
    {
    }
};

template< typename T >
struct Domain {
    int m_id;
    std::map< T*, int > m_ids;

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

    Barrier< T > m_barrier;
    Parallel *m_parallel;
    Parallel &parallel() {
        if ( !m_parallel ) {
            // FIXME parametrize over number of workers
            m_parallel = new Parallel( this, self(), 10 );
            m_barrier.setExpect( 10 ); // FIXME 10 again
            for ( int i = 0; i < m_parallel->n; ++i ) {
                m_parallel->instance( i ).connect( self() );
            }
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
