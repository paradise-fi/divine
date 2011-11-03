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
        assert_eq( size_t( n ), m_instances.size() );
        return m_instances[ i ];
    }

    /// Look up i-th instance's shared data.
    Shared &shared( int i ) {
        return instance( i ).shared;
    }

    /// Look up i-th thread (as opposed to i-th instance).
    R< T > &thread( int i ) {
        assert( i < n );
        assert_eq( size_t( n ), m_threads.size() );
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

template< typename, typename > struct Domain;

/**
 * A building block of two-dimensional communication matrix primitive. The
 * resulting matrix, for size n, has n^2 Fifo instances, one for each direction
 * of communication for each pair of communicating tasks (threads). The Fifo
 * instances are relatively cheap to instantiate.
 *
 * This class in itself is a single row of such a matrix, representing
 * *incoming* queues for a single thread. See also Domain and DomainWorker.
 */
template< typename _T >
struct FifoMatrix
{
    typedef _T T;
    typedef divine::Fifo< T > Fifo;
    std::vector< std::vector< Fifo > > m_matrix;

    void validate( int from, int to ) {
        assert_leq( from, m_matrix.size() - 1 );
        assert_leq( to, m_matrix[ from ].size() - 1 );
    }

    bool pending( int from, int to ) {
        validate( from, to );
        return !m_matrix[ from ][ to ].empty();
    };

    bool pending( int to ) {
        for ( int from = 0; size_t( from ) < m_matrix.size(); ++from )
            if ( pending( from, to ) )
                return true;
        return false;
    }

    void submit( int from, int to, T value ) {
        validate( from, to );
        m_matrix[ from ][ to ].push( value );
    }

    T take( int from, int to ) {
        validate( from, to );
        T r = m_matrix[ from ][ to ].front();
        m_matrix[ from ][ to ].pop();
        return r;
    }

    T take( int to ) {
        for ( int from = 0; size_t( from ) < m_matrix.size(); ++from )
            if ( pending( from, to ) )
                return take( from, to );
        assert_die();
    }

    void resize( int size ) {
        m_matrix.resize( size );
        for ( int i = 0; i < size; ++i )
            m_matrix[ i ].resize( size );
    }
};

typedef std::pair<Blob, Blob> BlobPair;

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
template< typename T, typename Comms = FifoMatrix< BlobPair > >
struct DomainWorker {
    typedef divine::Fifo< Blob > Fifo;
    typedef wibble::Unit IsDomainWorker;

    Domain< T, Comms > *m_domain;
    bool is_master;
    int m_id;
    bool m_interrupt, m_busy;

    DomainWorker()
        : m_domain( 0 ), is_master( false ), m_interrupt( false ), m_busy( true )
    {}

    DomainWorker( const DomainWorker & )
        : m_domain( 0 ), is_master( false ), m_interrupt( false ), m_busy( true )
    {}

    template< typename Shared >
    void becomeMaster( Shared *shared = 0, int n = 4 ) {
        is_master = true;
        assert( !m_domain );
        m_domain = new Domain< T, Comms >( shared, n );
    }

    Domain< T, Comms > &master() {
        return domain();
    }

    Domain< T, Comms > &domain() {
        assert( m_domain );
        return *m_domain;
    }

    void connect( Domain< T, Comms > &dom ) {
        assert( !m_domain );
        m_domain = &dom;
        m_id = dom.obtainId( *this );
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
        for ( int from = 0; from < peers(); ++from )
            if ( master().comms.pending( from, globalId() ) )
                return true;
        return false;
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

    /// Submit a message.
    void submit( int from, int to, typename Comms::T value ) {
        assert( from < peers() );
        return master().submit( from, to, value );
    }

    Comms &comms() {
        return master().comms;
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
template< typename T, typename _Comms = FifoMatrix< BlobPair > >
struct Domain {
    typedef _Comms Comms;
    Comms comms;

    struct Parallel : divine::Parallel< T, BarrierThread >
    {
        Domain< T, Comms > *m_domain;
        MpiWorker< Domain< T, Comms >, Comms > mpiWorker;

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
            if (m_domain->mpi.size() > 1 )
                mpiWorker.join();
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

        Parallel( Domain< T, Comms > &dom, int _n )
            : divine::Parallel< T, BarrierThread >( _n ),
              m_domain( &dom ), mpiWorker( dom )
        {
        }
    };

    Mpi< T, Domain< T, Comms > > mpi;
    Barrier< Terminable > m_barrier;

    int minId, maxId, lastId;
    std::map< DomainWorker< T, Comms >*, int > m_ids;
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
        comms.resize( peers() ); // FIXME kinda out of place, here, but in the
                                 // ctor we don't have the right peers() figure
                                 // yet :(
        minId = lastId = n * mpi.rank();
        maxId = (n * (mpi.rank() + 1)) - 1;
    }

    int obtainId( DomainWorker< T, Comms > &t ) {
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

    void submit( int from, int to, typename Comms::T value ) {
        comms.submit( from, to, value );
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
