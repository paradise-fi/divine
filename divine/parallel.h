// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <wibble/sys/thread.h>
#include <divine/fifo.h>
#include <divine/blob.h>
#include <divine/barrier.h>
#include <divine/mpi.h>

#ifndef DIVINE_PARALLEL_H
#define DIVINE_PARALLEL_H

namespace divine {

/**
 * A simple structure that runs a method of a class in a separate thread.
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

/**
 * The basic structure for implementing (shared-memory) parallelism. The
 * Parallel< T > class implements a simple model of thread-based
 * parallelism. Each thread executes the same code, and the caller blocks until
 * all the threads are finished.
 *
 * The type T is expected to include a nested typedef, Shared -- a value of
 * this Shared type is then, at the start of each parallel execution,
 * distributed to each of the threads (cf. the run() method). The number of
 * (identical) threads to execute is passed in as a constructor parameter.
 */
template< typename T, template< typename > class R = RunThread >
struct Parallel {
    typedef typename T::Shared Shared;
    std::vector< T > m_instances;
    std::vector< R< T > > m_threads;

    int n;

    T &instance( int i ) {
        assert( i < n );
        assert_eq( n, m_instances.size() );
        return m_instances[ i ];
    }

    /// Look up i-th instance's shared data.
    Shared &shared( int i ) {
        return instance( i ).shared;
    }

    /// Look up i-th thread (as opposed to i-th instance).
    R< T > &thread( int i ) {
        assert( i < n );
        assert_eq( n, m_threads.size() );
        return m_threads[ i ];
    }

    /// Internal.
    template< typename Shared, typename F >
    void initThreads( Shared &sh, F f ) {
        m_threads.clear();
        for ( int i = 0; i < n; ++i ) {
            instance( i ).shared = sh;
            m_threads.push_back( R< T >( instance( i ), f ) );
        }
    }

    /// Internal.
    void runThreads() {
        for ( int i = 0; i < n; ++i )
            thread( i ).start();
        for ( int i = 0; i < n; ++i )
            thread( i ).join();
    }

    template< typename F >
    void run( Shared &sh, F f ) {
        initThreads( sh, f );
        runThreads();
    }

    Parallel( int _n ) : n( _n )
    {
        m_instances.resize( n );
    }
};

/**
 * Straightforward extension of RunThread -- additionally, this one registers
 * with a Barrier, for distributed termination detection.
 */
template< typename T >
struct BarrierThread : RunThread< T >, Terminable {
    Barrier< Terminable > *m_barrier;

    void setBarrier( Barrier< Terminable > &b ) {
        m_barrier = &b;
    }

    virtual void init() {
        assert( m_barrier );
        m_barrier->started( this );
    }

    virtual void fini() {
        m_barrier->done( this );
    }

    bool workWaiting() {
        return this->t->workWaiting();
    }

    bool isBusy() { return this->t->isBusy(); }

    BarrierThread( T &_t, typename RunThread< T >::F _f )
        : RunThread< T >( _t, _f ), m_barrier( 0 )
    {
    }
};

template< typename > struct Domain;

/**
 * A building block of two-dimensional communication matrix primitive. The
 * resulting matrix, for size n, has n^2 Fifo instances, one for each direction
 * of communication for each pair of communicating tasks (threads). The Fifo
 * instances are relatively cheap to instantiate.
 *
 * This class in itself is a single row of such a matrix, representing
 * *incoming* queues for a single thread. See also Domain and DomaniWorker.
 */
template< typename T >
struct FifoVector
{
    int m_last;
    typedef divine::Fifo< T > Fifo;
    std::vector< Fifo > m_vector;

    bool empty() {
        for ( int i = 0; i < m_vector.size(); ++i ) {
            Fifo &fifo = m_vector[ (m_last + i) % m_vector.size() ];
            if ( !fifo.empty() )
                return false;
        }
        return true;
    };

    T &next( bool wait = false ) {
        if ( wait )
            return m_vector[ m_last ].front( wait );

        while ( m_vector[ m_last ].empty() )
            m_last = (m_last + 1) % m_vector.size();
        return m_vector[ m_last ].front( wait );
    }

    Fifo &operator[]( int i ) {
        assert_leq( i, m_vector.size() );
        return m_vector[ i ];
    }

    void remove() {
        while ( m_vector[ m_last ].empty() )
            m_last = (m_last + 1) % m_vector.size();
        m_vector[ m_last ].pop();
    }

    void resize( int i ) { m_vector.resize( i, Fifo() ); }
    FifoVector() : m_last( 0 ) {}
};

/**
 * A basic template of a worker object, as a part of a larger work
 * Domain. Provides communication, distributed termination detection and clean
 * early termination (interrupt). You usually want to derive your parallel
 * workers from this class. See e.g. reachability.h for usage example.
 *
 * Usually, one of a group of DomainWorker-derived instances is the
 * master. NB. The master does no parallel work. The master is expected to
 * coordinate the high-level serial structure of the algorithm, and call into
 * parallel sections as needed, using domain().parallel().run( ... ). The
 * master is expected to call becomeMaster( ... ) in its constructor.
 */
template< typename T >
struct DomainWorker {
    typedef divine::Fifo< Blob > Fifo;

    typedef wibble::Unit IsDomainWorker;

    Domain< T > *m_domain;
    bool is_master;
    FifoVector< Blob > fifo;
    int m_id;
    bool m_interrupt, m_busy;

    DomainWorker()
        : m_domain( 0 ), is_master( false ), m_interrupt( false ), m_busy( true )
    {}

    DomainWorker( const DomainWorker &o )
        : m_domain( 0 ), is_master( false ), m_interrupt( false ), m_busy( true )
    {}

    template< typename Shared >
    void becomeMaster( Shared *shared = 0, int n = 4 ) {
        is_master = true;
        assert( !m_domain );
        m_domain = new Domain< T >( shared, n );
    }

    Domain< T > &master() {
        return domain();
    }

    Domain< T > &domain() {
        assert( m_domain );
        return *m_domain;
    }

    void connect( Domain< T > &dom ) {
        assert( !m_domain );
        m_domain = &dom;
        m_id = dom.obtainId( *this );
        // FIXME this whole fifo allocation business is an ugly hack...
        fifo.resize( peers() + 1 );
    }

    int peers() {
        return master().n * master().mpi.size();
    }

    bool idle() {
        m_busy = false;
        m_interrupt = false;
        bool res = master().barrier().idle( terminable() );
        m_busy = true;
        return res;
    }

    bool isBusy() { return m_busy; }

    bool workWaiting() {
        return !this->fifo.empty();
    }

    int globalId() {
        assert( m_domain );
        return m_id;
    }

    int localId() {
        return m_id - master().minId;
    }

    /// Terminate early. Notifies peers (always call without a parameter!).
    void interrupt( bool from_master = false ) {
        m_interrupt = true;
        if ( !from_master )
            master().interrupt();
    }

    bool interrupted() {
        return m_interrupt;
    }

    /// Restart (i.e. continue) computation (after termination has happened).
    void restart() {
        m_interrupt = false;
        m_busy = true;
        master().parallel().m_threads[ localId() ].m_barrier->started( terminable() );
    }

    /// Internal.
    Terminable *terminable() {
        return &master().parallel().m_threads[ localId() ];
    }

    /// Find a queue used for passing messages between a pair of workers.
    Fifo &queue( int from, int to ) {
        assert( from < peers() );
        return master().queue( from, to );
    }

    ~DomainWorker() {
        if ( is_master )
            delete m_domain;
    }
};

/**
 * A parallel Domain. This is the control object, maintained (automatically) by
 * the master. Cf. DomainWorker. You should not need to instantiate this
 * manually.
 */
template< typename T >
struct Domain {
    typedef divine::Fifo< Blob > Fifo;

    struct Parallel : divine::Parallel< T, BarrierThread >
    {
        Domain< T > *m_domain;
        MpiWorker< Domain< T > > mpiWorker;

        template< typename Shared, typename F >
        void run( Shared &sh, F f ) {
            this->initThreads( sh, f );

            for ( int i = 0; i < this->n; ++i )
                this->thread( i ).setBarrier( m_domain->barrier() );

            m_domain->mpi.runOnSlaves( sh, f );
            if (m_domain->mpi.size() > 1 )
                mpiWorker.start();
            this->runThreads();
            m_domain->mpi.collectSharedBits(); // wait for shared stuff
            m_domain->barrier().clear();
        }

        template< typename Shared, typename F >
        void runInRing( Shared &sh, F f ) {
            this->shared( 0 ) = sh;
            for ( int i = 0; i < this->n; ++i ) {
                ((this->instance( i )).*f)();
                if ( i < this->n - 1 ) {
                    this->shared( i + 1 ) = this->shared( i );
                }
                if ( i == this->n - 1 )
                    sh = this->shared( i );
            }
            m_domain->mpi.runInRing( f );
        }

        Parallel( Domain< T > &dom, int _n )
            : divine::Parallel< T, BarrierThread >( _n ),
              m_domain( &dom ), mpiWorker( dom )
        {
        }
    };


    Mpi< T, Domain< T > > mpi;
    Barrier< Terminable > m_barrier;

    int minId, maxId, lastId;
    std::map< DomainWorker< T >*, int > m_ids;
    Parallel *m_parallel;

    int n;

    Barrier< Terminable > &barrier() {
        return m_barrier;
    }

    Parallel &parallel() {
        if ( !m_parallel ) {
            m_parallel = new Parallel( *this, n );

            int count = n;
            if ( mpi.size() > 1 )
                ++ count;

            m_barrier.setExpect( count );

            for ( int i = 0; i < m_parallel->n; ++i ) {
                m_parallel->instance( i ).connect( *this );
            }
        }
        return *m_parallel;
    }

    void setupIds() {
        minId = lastId = n * mpi.rank();
        maxId = (n * (mpi.rank() + 1)) - 1;
    }

    int obtainId( DomainWorker< T > &t ) {
        if ( !lastId ) {
            setupIds();
        }

        if ( !m_ids.count( &t ) )
            m_ids[ &t ] = lastId ++;

        return m_ids[ &t ];
    }

    bool isLocalId( int id ) {
        return id >= minId && id <= maxId;
    }

    int peers() {
        return n * mpi.size();
    }

    typename T::Shared &shared( int i ) {
        if ( isLocalId( i ) )
            return parallel().shared( i - minId );
        return mpi.shared( i );
    }

    void interrupt( bool from_mpi = false ) {
        for ( int i = 0; i < parallel().n; ++i )
            parallel().instance( i ).interrupt( true );
        if ( !from_mpi )
            parallel().mpiWorker.interrupt();
    }

    Fifo &queue( int from, int to )
    {
        if ( isLocalId( to ) ) {
            barrier().wakeup( &parallel().thread( to - minId ) );
            return parallel().instance( to - minId ).fifo[ from + 1 ];
        } else
            return parallel().mpiWorker.fifo[ from + to * peers() ];
    }

    Domain( typename T::Shared *shared = 0, int _n = 4 )
        : mpi( shared, this ),
          lastId( 0 ),
          m_parallel( 0 ),
          n( _n )
    {}

    ~Domain() {
        delete m_parallel;
    }
};

}

#endif
