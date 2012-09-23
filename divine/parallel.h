// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <wibble/sys/thread.h>
#include <divine/filefifo.h>
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
template< typename T, typename R = RunThread< T > >
struct ThreadVector {
    std::vector< R > m_threads;

    R &thread( int i ) {
        assert_leq( i, m_threads.size() - 1 );
        return m_threads[ i ];
    }

    void run( wibble::sys::Thread *extra = 0 ) {
        size_t n = m_threads.size();
        for ( int i = 0; i < n; ++i )
            m_threads[ i ].start();
        if ( extra )
            extra->start();
        for ( int i = 0; i < n; ++i )
            m_threads[ i ].join();
        if ( extra )
            extra->join();
    }

    ThreadVector( std::vector< T > &instances, void (T::*fun)() )
    {
        for ( int i = 0; i < instances.size(); ++ i )
            m_threads.push_back( R( instances[ i ], fun ) );
    }

    ThreadVector() {}
};

/**
 * Straightforward extension of RunThread -- additionally, this one registers
 * with a Barrier, for distributed termination detection.
 */
template< typename T >
struct BarrierThread : RunThread< T > {
    Barrier< Terminable > *m_barrier;

    void setBarrier( Barrier< Terminable > &b ) {
        m_barrier = &b;
    }

    virtual void init() {
        assert( m_barrier );
        m_barrier->started( this->t );
    }

    virtual void fini() {
        m_barrier->done( this->t );
    }

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
 * See also Parallel and LocalTopology.
 */
template< typename _T >
struct FifoMatrix
{
    typedef _T T;
    typedef divine::Fifo< T > Fifo;
    std::vector< std::vector< Fifo > > m_matrix;
    int fifoParam;

    void validate( int from, int to ) {
        assert_leq( from, int( m_matrix.size() ) - 1 );
        assert_leq( to, int( m_matrix[ from ].size() ) - 1 );
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

    void setup( int param ) {
        fifoParam = param;
    }

    void notify( int workerId, Pool *pool ) {
        /* for ( int i = 0; i < m_matrix.size(); ++i ) {
            m_matrix[ workerId ][ i ].setup( fifoParam, pool, NULL );
            m_matrix[ i ][ workerId ].setup( fifoParam, NULL, pool );
        } */
    }
};

typedef std::pair<Blob, Blob> BlobPair;

struct WithID {
    int m_id;
    int id() { assert_leq( 0, m_id ); return m_id; }
    WithID() : m_id( -1 ) {}
};

struct Sequential : WithID {
    Sequential() { m_id = 0; }
};

/**
 * A basic template for an object with parallel sections, which are arranged
 * according to a parametric topology. The topology is responsible for setting
 * up instances and providing means of communication and synchronisation. See
 * e.g. reachability.h for usage example.
 *
 * One instance of your derived class is a "master" which does no parallel work
 * itself, but coordinates the high-level serial structure of the
 * algorithm. The master invokes parallel sections of the algorithm, which are
 * then executed through the topology (see parallel()). The master must call
 * becomeMaster() in its constructor.
 */
template< template < typename > class Topology, typename Instance >
struct Parallel : Terminable, WithID {
    typedef wibble::Unit IsParallel;

    Topology< Instance > *m_topology;
    typedef typename Topology< Instance >::Comms Comms;

    bool is_master;
    bool m_interrupt, m_busy;

    Parallel()
        : m_topology( 0 ), is_master( false ), m_interrupt( false ), m_busy( true )
    {}

    Parallel( const Parallel & ) /* fake copy */
        : m_topology( 0 ), is_master( false ), m_interrupt( false ), m_busy( true )
    {}

    template< typename M = Instance >
    void becomeMaster( int n, M m = M() ) {
        is_master = true;
        assert( !m_topology );
        m_topology = new Topology< Instance >( n, m ); // TODO int is kind of limited
    }

    void becomeSlave( Topology< Instance > &topology, int id ) {
        m_topology = &topology;
        m_id = id;
    }

    Topology< Instance > &topology() {
        assert( m_topology );
        return *m_topology;
    }

    int peers() {
        return topology().peers();
    }

    bool idle() {
        assert( !is_master );
        m_busy = false;
        m_interrupt = false;
        bool res = topology().barrier().idle( static_cast< Instance* >( this ) );
        m_busy = true;
        return res;
    }

    bool isBusy() { return m_busy; }

    bool workWaiting() {
        for ( int from = 0; from < peers(); ++from )
            if ( comms().pending( from, id() ) )
                return true;
        return false;
    }

    /// Terminate early. Notifies peers (always call without a parameter!).
    void interrupt( bool from_master = false ) {
        m_interrupt = true;
        if ( !from_master )
            topology().interrupt();
    }

    bool interrupted() {
        return m_interrupt;
    }

    /// Restart (i.e. continue) computation (after termination has happened).
    void restart() {
        m_interrupt = false;
        m_busy = true;
        topology().barrier().started( static_cast< Instance * >( this ) );
    }

    /// Submit a message.
    void submit( int from, int to, typename Comms::T value ) {
        assert( from < peers() );
        return comms().submit( from, to, value );
    }

    Comms &comms() {
        return topology().comms();
    }

    ~Parallel() {
        if ( is_master )
            delete m_topology;
    }
};

template< typename Message = BlobPair >
struct Topology {

template< typename Instance >
struct Local
{
    typedef ThreadVector< Instance, BarrierThread< Instance > > Threads;
    typedef std::vector< Instance > Instances;
    typedef FifoMatrix< Message > Comms;

    Instances m_slaves;
    Barrier< Terminable > m_barrier;
    Comms m_comms;

    Comms &comms() { return m_comms; }
    Barrier< Terminable > &barrier() { return m_barrier; }

    template< typename X = Instance >
    Local( int n, X init = X() ) {
        for ( int i = 0; i < n; ++ i )
            m_slaves.push_back( Instance( init ) );
        m_comms.resize( n );
    }

    template< typename Base, typename Bit >
    auto distribute( Bit bit, void (Base::*set)( Bit ) )
         -> decltype( rpc::check_void< void (Instance::*)( Bit ) >( set ) )
    {
        for ( int i = 0; i < m_slaves.size(); ++i )
            (m_slaves[ i ].*set)( bit );
    }

    template< typename Base, typename Bits, typename Bit >
    auto collect( Bits &bits, Bit (Base::*get)() )
         -> decltype( rpc::check_void< Bit (Instance::*)() >( get ) )
    {
        for ( int i = 0; i < m_slaves.size(); ++i )
            bits.push_back( (m_slaves[ i ].*get)() );
    }

    int peers() { return m_slaves.size(); }

    auto parallel( void (Instance::*fun)() )
         -> decltype( rpc::check_void< void (Instance::*)() >( fun ) )
    {
        parallel( this, fun );
    }

    template< typename Self >
    void parallel( Self *self, void (Instance::*fun)(),
                   wibble::sys::Thread * extra = 0, int offset = 0 )
    {
        int nextra = extra ? 1 : 0;
        Threads threads( m_slaves, fun );
        m_barrier.setExpect( m_slaves.size() + nextra );

        for ( int i = 0; i < m_slaves.size(); ++i ) {
            threads.thread( i ).setBarrier( m_barrier );
            m_slaves[ i ].becomeSlave( *self, offset + i );
        }

        threads.run( extra );
    }

    template< typename X >
    X ring( X x, X (Instance::*fun)( X ) )
    {
        for ( int i = 0; i < m_slaves.size(); ++i )
            x = (m_slaves[ i ].*fun)( x );
        return x;
    }

    void interrupt() {
        for ( int i = 0; i < m_slaves.size(); ++i )
            m_slaves[ i ].interrupt( true );
    }
};

template< typename Instance >
struct Mpi : MpiMonitor
{
    typedef FifoMatrix< BlobPair > Comms;

    divine::Mpi mpi;
    Local< Instance > m_local;
    MpiForwarder< Comms > m_mpiForwarder;
    bitstream async_retval;
    int request_source;

    Comms &comms() { return m_local.comms(); }
    Barrier< Terminable > &barrier() { return m_local.barrier(); }

    int peers() { return m_local.peers() * mpi.size(); } // XXX

    Mpi( const Mpi& ) = delete;

    template< typename X = Instance >
    Mpi( int pernode, X init = X() )
        : m_local( pernode, init ),
          m_mpiForwarder( &barrier(),
                          &comms(),
                          pernode * mpi.size(), /* total */
                          mpi.rank() * pernode, /* min */
                          (mpi.rank() + 1) * pernode - 1 /* max */
                        )
    {
        mpi.debug() << "created MpiTopology, pernode = " << pernode << ", size = " << mpi.size() << std::endl;
        mpi.registerMonitor( TAG_RING, *this );
        mpi.registerMonitor( TAG_PARALLEL, *this );
        mpi.registerMonitor( TAG_INTERRUPT, *this );
        comms().resize( pernode * mpi.size() );
    }

    template< typename Bit >
    void distribute( Bit bit, void (Instance::*set)( Bit ) )
    {
        bitstream bs;
        rpc::marshall( set, bit, bs );
        wibble::sys::MutexLock _lock( mpi.global().mutex );
        mpi.notifySlaves( _lock, TAG_PARALLEL, bs );
        m_local.distribute( bit, set );
    }

    template< typename Bits, typename Bit >
    void collect( Bits &bits, Bit (Instance::*get)() )
    {
        m_local.collect( bits, get );

        bitstream bs;
        rpc::marshall( get, bs );
        if ( mpi.master() ) {
            wibble::sys::MutexLock _lock( mpi.global().mutex );
            mpi.notifySlaves( _lock, TAG_PARALLEL, bs );
            for ( int i = 1; i < mpi.size(); ++i ) {
                bitblock response;
                mpi.getStream( _lock, mpi.anySource, TAG_COLLECT, response );
                response >> bits;
            }
        }
    }

    void parallel( void (Instance::*fun)() )
    {
        bitstream bs;
        rpc::marshall( fun, bs );

        {
            wibble::sys::MutexLock _lock( mpi.global().mutex );
            mpi.notifySlaves( _lock, TAG_PARALLEL, bs );
        }

        mpi.debug() << "parallel()" << std::endl;
        m_local.parallel( this, fun,
                          mpi.size() > 1 ? &m_mpiForwarder : 0,
                          mpi.rank() * m_local.peers() );
        mpi.debug() << "parallel() DONE" << std::endl;
    }

    template< typename X >
    X ring( X x, X (Instance::*fun)( X ) ) {
        X retval;
        bitstream bs;

        retval = x = m_local.ring( x, fun );
        rpc::marshall( fun, x, bs );

        wibble::sys::MutexLock _lock( mpi.global().mutex );
        mpi.global().is_master = false;
        mpi.sendStream( _lock, bs, (mpi.rank() + 1) % mpi.size(), TAG_RING );
        _lock.drop();

        if ( mpi.size() > 1 ) {
            while ( mpi.loop() == Continue ) ; // wait (and serve) till the ring is done
            async_retval >> retval;
        }

        mpi.global().is_master = true;
        return retval;
    }

    template< typename MPIT, typename F > struct RingFromRemote
    {
        template< typename X >
        auto operator()( MPIT &mpit, X (Instance::*fun)( X ), X x ) -> NOT_VOID( X )
        {
            if ( !mpit.mpi.rank() )
                mpit.async_retval << x; /* done, x is the final value */
            else {
                mpit.mpi.global().is_master = true;
                mpit.ring( x, fun );
                mpit.mpi.global().is_master = false;
            }
        }
    };

    template< typename MPIT, typename F > struct ParallelFromRemote
    {
        template< typename X >
        auto operator()( MPIT &mpit, void (Instance::*fun)( X ), X x ) -> NOT_VOID( X )
        {
            mpit.mpi.debug() << "MPI: distribute()" << std::endl;
            mpit.distribute( x, fun );
        }

        template< typename X >
        void operator()( MPIT &mpit, X (Instance::*fun)() )
        {
            std::vector< X > bits;
            bitstream bs;
            mpit.collect( bits, fun );
            bs << bits;

            wibble::sys::MutexLock _lock( mpit.mpi.global().mutex );
            mpit.mpi.sendStream( _lock, bs, mpit.request_source, TAG_COLLECT );
        }

        void operator()( MPIT &mpit, void (Instance::*fun)() ) {
            mpit.parallel( fun );
        }

    };

    /* The slave monitor */
    Loop process( wibble::sys::MutexLock &_lock, divine::Mpi::Status &status ) {

        bitblock in;
        bitstream out;

        mpi.debug() << "slave( tag = " << status.Get_tag() << " )" << std::endl;
        mpi.recvStream( _lock, status, in );
        request_source = status.Get_source();

        switch ( status.Get_tag() ) {
            case TAG_RING:
                _lock.drop();
                rpc::demarshallWith< Instance, RingFromRemote >( *this, in, out );
                if ( !async_retval.empty() )
                    return Done;
                break;
            case TAG_PARALLEL:
                _lock.drop();
                rpc::demarshallWith< Instance, ParallelFromRemote >( *this, in, out );
                break;
            case TAG_INTERRUPT:
                mpi.debug() << "INTERRUPTED (remote)" << std::endl;
                m_local.interrupt();
                break;
            default:
                assert_die();
        }

        return Continue;
    }

    void interrupt() {
        wibble::sys::MutexLock _lock( mpi.global().mutex );
        mpi.debug() << "INTERRUPTED (local)" << std::endl;
        mpi.notify( _lock, TAG_INTERRUPT );
    }

};

};


}

#endif
