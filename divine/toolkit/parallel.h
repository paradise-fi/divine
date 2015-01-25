// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>
//             (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <mutex>
#include <brick-types.h>
#include <brick-shmem.h>
#include <divine/toolkit/barrier.h>
#include <divine/toolkit/mpi.h>
#include <divine/toolkit/rpc.h>

#ifndef DIVINE_PARALLEL_H
#define DIVINE_PARALLEL_H

namespace divine {

/**
 * A simple structure that runs a method of a class in a separate thread.
 */
template< typename T >
struct RunThread : brick::shmem::Thread {
    typedef void (T::*F)();
    T *t;
    F f;

    virtual void init() {}
    virtual void fini() {}

    void main() {
        init();
        (t->*f)();
        fini();
    }

    RunThread( T &_t, F _f ) : t( &_t ), f( _f )
    { }
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
        ASSERT_LEQ( i, int( m_threads.size() ) - 1 );
        return m_threads[ i ];
    }

    void run( brick::shmem::Thread *extra = 0 ) {
        int n = m_threads.size();
        for ( int i = 0; i < n; ++i )
            m_threads[ i ].start();
        if ( extra )
            extra->start();
        for ( int i = 0; i < n; ++i )
            m_threads[ i ].join();
        if ( extra )
            extra->join();
    }

    template< typename I >
    ThreadVector( std::vector< I > &instances, void (T::*fun)() )
    {
        for ( int i = 0; i < int( instances.size() ); ++ i )
            m_threads.emplace_back( *static_cast< T * >( &instances[ i ] ), fun );
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
        ASSERT( m_barrier );
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
    typedef brick::shmem::Fifo< T > Fifo;
    std::vector< std::vector< Fifo > > m_matrix;

    void validate( int from, int to ) {
        ASSERT_LEQ( from, int( m_matrix.size() ) - 1 );
        ASSERT_LEQ( to, int( m_matrix[ from ].size() ) - 1 );
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
        ASSERT_UNREACHABLE_F( "take on %d unexpectedly failed", to );
    }

    void resize( int size ) {
        m_matrix.resize( size );
        for ( int i = 0; i < size; ++i )
            m_matrix[ i ].resize( size );
    }

    /* void enableSaving( bool enable = true ) {
        for ( auto& v : m_matrix ) {
            for ( auto& fifo : v ) {
                fifo.enableSaving( enable );
            }
        }
        } */
};

struct WithID {
    int _localId;
    int _id;
    int _peers;
    int _locals;
    int _rank;
    int localId() const { ASSERT_LEQ( 0, _localId ); return _localId; }
    int id() const { ASSERT_LEQ( 0, _id ); return _id; }
    int peers() const { ASSERT_LEQ( 0, _id ); return _peers; }
    int rank() const { ASSERT_LEQ( 0, _id ); return _rank; }
    int locals() const { ASSERT_LEQ( 0, _locals ); return _locals; }
    void setId( int localId, int id, int peers, int locals, int rank ) {
        _localId = localId;
        _id = id;
        _peers = peers;
        _locals = locals;
        _rank = rank;
    }
    WithID() : _id( -1 ) {}
    WithID( std::pair< int, int > id, int peers, int locals, int rank ) :
        _localId( id.first ), _id( id.second ), _peers( peers ), _locals( locals ), _rank( rank )
    { }
};

struct Sequential : WithID {
    Sequential() { setId( 0, 0, 1, 1, 0 ); }

    Pool m_pool;
    const Pool& masterPool() const {
        return m_pool;
    }

    template< typename... Args >
    void becomeMaster( const Args&... ) { }
    template< typename... Args >
    void initSlaves( const Args&... ) { }
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
    typedef brick::types::Unit IsParallel;

    Topology< Instance > *m_topology;
    typedef typename Topology< Instance >::Comms Comms;

    bool is_master;
    bool m_interrupt, m_busy;

    Parallel()
        : m_topology( 0 ), is_master( false ), m_interrupt( false ), m_busy( true )
    {}

    Parallel( const Parallel & ) /* fake copy */
        : WithID(), m_topology( 0 ), is_master( false ), m_interrupt( false ), m_busy( true )
    {}

    template< typename M = Instance >
    void becomeMaster( int n ) {
        is_master = true;
        ASSERT( !m_topology );
        m_topology = new Topology< Instance >( n ); // TODO int is kind of limited
        setId( -1, -1 , -1, -1, m_topology->rank() ); /* try to catch anyone thinking to use our ID */
    }

    void becomeSlave( Topology< Instance > &topology, std::pair< int, int > id ) {
        m_topology = &topology;
        setId( id.first, id.second, m_topology->peers(), m_topology->locals(), m_topology->rank() );
    }

    template< typename X = Instance >
    void initSlaves( X &init ) {
        topology().initSlaves( init );
    }

    Topology< Instance > &topology() {
        ASSERT( m_topology );
        return *m_topology;
    }

    // the original pool on machine
    const Pool &masterPool() const {
        ASSERT( m_topology );
        return m_topology->masterPool();
    }

    int peers() {
        return topology().peers();
    }

    bool idle() {
        ASSERT( !is_master );
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
        ASSERT( from < peers() );
        comms().submit( from, to, value );
        topology().wakeup( to );
    }

    Comms &comms() {
        return topology().comms();
    }

    ~Parallel() {
        if ( is_master )
            delete m_topology;
    }
};

template< typename Message >
struct Topology {

template< typename Instance >
struct Local
{
    template< typename Inst >
    using Threads = ThreadVector< Inst, BarrierThread< Inst > >;
    typedef std::vector< Instance > Instances;
    typedef FifoMatrix< Message > Comms;

    Pool m_pool;
    Instances m_slaves;
    Barrier< Terminable > m_barrier;
    Comms m_comms;
    int m_slavesCount;
    int m_offset;

    Comms &comms() { return m_comms; }
    Barrier< Terminable > &barrier() { return m_barrier; }

    Local( int n, int offset = 0 ) :
        m_slavesCount( n ), m_offset( offset )
    {
        m_slaves.reserve( n ); /* avoid reallocation at all costs! */
        m_comms.resize( n );
    }

    template< typename X = Instance >
    void initSlaves( X &init ) {
        for ( int i = 0; i < m_slavesCount; ++ i )
            m_slaves.emplace_back( static_cast< Instance & >( init ), std::make_pair( i, m_offset + i ) );
    }

    const Pool &masterPool() const {
        return m_pool;
    }

    int rank() {
        return 0;
    }

    template< typename T >
    static void _check( T t );

    template< typename Base, typename Bit >
    auto distribute( Bit bit, void (Base::*set)( Bit ) )
         -> decltype( _check< void (Instance::*)( Bit ) >( set ) )
    {
        for ( int i = 0; i < int( m_slaves.size() ); ++i )
            (m_slaves[ i ].*set)( bit );
    }

    template< typename Base, typename Bits, typename Bit >
    auto collect( Bits &bits, Bit (Base::*get)() )
         -> decltype( _check< Bit (Instance::*)() >( get ) )
    {
        for ( int i = 0; i < int( m_slaves.size() ); ++i )
            bits.push_back( (m_slaves[ i ].*get)() );
    }

    int peers() { return m_slavesCount; }
    int locals() { return m_slavesCount; }
    void wakeup( int id ) { barrier().wakeup( &m_slaves[ id ] ); }

    template< typename I >
    auto parallel( void (I::*fun)() )
         -> decltype( _check< void (Instance::*)() >( fun ) )
    {
        parallel( this, fun );
    }

    template< typename Self, typename Inst >
    void parallel( Self *, void (Inst::*fun)(),
                   brick::shmem::Thread * extra = 0 )
    {
        static_assert( std::is_base_of< Inst, Instance >::value,
                "Trying to call unrellated function, that is not supported." );
        int nextra = extra ? 1 : 0;
        Threads< Inst > threads( m_slaves, fun );
        m_barrier.setExpect( m_slaves.size() + nextra );

        for ( int i = 0; i < int( m_slaves.size() ); ++i )
            threads.thread( i ).setBarrier( m_barrier );

        threads.run( extra );
    }

    template< typename X, typename Inst >
    X ring( X x, X (Inst::*fun)( X ) )
    {
        static_assert( std::is_base_of< Inst, Instance >::value,
                "Trying to call unrellated function, that is not supported." );
        for ( int i = 0; i < int( m_slaves.size() ); ++i )
            x = (static_cast< Inst * >( &m_slaves[ i ] )->*fun)( x );
        return x;
    }

    void interrupt() {
        for ( int i = 0; i < int( m_slaves.size() ); ++i )
            m_slaves[ i ].interrupt( true );
    }
};

template< typename Instance >
struct Mpi : MpiMonitor
{
    typedef FifoMatrix< Message > Comms;

    divine::Mpi mpi;
    Local< Instance > m_local;
    MpiForwarder< Comms > m_mpiForwarder;
    bitblock async_retval;
    int request_source;
    int _pernode;

    Comms &comms() { return m_local.comms(); }
    Barrier< Terminable > &barrier() { return m_local.barrier(); }

    int peers() { return m_local.peers() * mpi.size(); } // XXX
    int locals() { return m_local.peers(); }
    void wakeup( int id ) {
        int start = mpi.rank() * _pernode, end = start + _pernode;
        if ( id >= start && id < end )
            m_local.wakeup( id - start );
    }

    Mpi( const Mpi& ) = delete;

    template< typename X = Instance >
    Mpi( int pernode )
        : m_local( pernode, mpi.rank() * pernode ),
          m_mpiForwarder( masterPool(),
                          &barrier(),
                          &comms(),
                          pernode * mpi.size(), /* total */
                          mpi.rank() * pernode, /* min */
                          (mpi.rank() + 1) * pernode - 1 /* max */
                        ),
          async_retval( m_mpiForwarder.pool )
    {
        mpi.registerMonitor( TAG_RING, *this );
        mpi.registerMonitor( TAG_PARALLEL, *this );
        mpi.registerMonitor( TAG_INTERRUPT, *this );
        comms().resize( pernode * mpi.size() );
        _pernode = pernode;
    }

    template< typename X = Instance >
    void initSlaves( X &init ) {
        m_local.initSlaves( init );
    }

    const Pool &masterPool() const {
        return m_local.masterPool();
    }

    int rank() {
        return mpi.rank();
    }

    template< typename Bit, typename I >
    void distribute( Bit bit, void (I::*set)( Bit ) )
    {
        if ( mpi.size() > 1 ) {
            bitblock bs( m_mpiForwarder.pool );
            rpc::marshall( bs, set, bit );
            std::unique_lock< std::mutex > _lock( mpi.global().mutex );
            mpi.notifySlaves( _lock, TAG_PARALLEL, bs );
        }
        m_local.distribute( bit, set );
    }

    template< typename Bits, typename Bit, typename I >
    void collect( Bits &bits, Bit (I::*get)() )
    {
        m_local.collect( bits, get );

        if ( mpi.size() == 1 )
            return;

        bitblock bs( m_mpiForwarder.pool );
        rpc::marshall( bs, get );
        if ( mpi.master() ) {
            std::unique_lock< std::mutex > _lock( mpi.global().mutex );
            mpi.notifySlaves( _lock, TAG_PARALLEL, bs );
            for ( int i = 1; i < mpi.size(); ++i ) {
                bitblock response( m_mpiForwarder.pool );
                mpi.getStream( _lock, mpi.anySource(), TAG_COLLECT, response );
                response >> bits;
            }
        }
    }

    template< typename I >
    void parallel( void (I::*fun)() )
    {
        bitblock bs( m_mpiForwarder.pool );
        rpc::marshall( bs, fun );

        {   std::unique_lock< std::mutex > _lock( mpi.global().mutex );
            mpi.notifySlaves( _lock, TAG_PARALLEL, bs );
        }

        m_local.parallel( this, fun,
                          mpi.size() > 1 ? &m_mpiForwarder : 0 );
    }

    template< typename X, typename I >
    X ring( X x, X (I::*fun)( X ) ) {
        X retval;
        bitblock bs( m_mpiForwarder.pool );

        retval = x = m_local.ring( x, fun );

        if ( mpi.size() > 1 ) {
            rpc::marshall( bs, fun, x );

            {   std::unique_lock< std::mutex > _lock( mpi.global().mutex );
                mpi.global().is_master = false;
                mpi.sendStream( _lock, bs, (mpi.rank() + 1) % mpi.size(), TAG_RING );
            }

            while ( mpi.loop() == Continue ) ; // wait (and serve) till the ring is done
            async_retval >> retval;
            mpi.global().is_master = true;
        }

        return retval;
    }

    template< typename MPIT, typename F > struct RingFromRemote
    {
        template< typename X, typename I >
        auto operator()( MPIT &mpit, X (I::*fun)( X ), X x )
            -> typename std::enable_if< !std::is_void< X >::value >::type
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
        template< typename X, typename I >
        auto operator()( MPIT &mpit, void (I::*fun)( X ), X x )
            -> typename std::enable_if< !std::is_void< X >::value >::type
        {
            mpit.distribute( x, fun );
        }

        template< typename X, typename I >
        void operator()( MPIT &mpit, X (I::*fun)() )
        {
            std::vector< X > bits;
            bitblock bs( mpit.m_mpiForwarder.pool );
            mpit.collect( bits, fun );
            bs << bits;

            std::unique_lock< std::mutex > _lock( mpit.mpi.global().mutex );
            mpit.mpi.sendStream( _lock, bs, mpit.request_source, TAG_COLLECT );
        }

        template< typename I >
        void operator()( MPIT &mpit, void (I::*fun)() ) {
            mpit.parallel( fun );
        }

    };

    /* The slave monitor */
    Loop process( std::unique_lock< std::mutex > &_lock, divine::Mpi::Status &status ) {

        bitblock in( m_mpiForwarder.pool ), out( m_mpiForwarder.pool );

        mpi.recvStream( _lock, status, in );
        request_source = status.Get_source();

        switch ( status.Get_tag() ) {
            case TAG_RING:
                _lock.unlock();
                rpc::demarshallWith< Instance, RingFromRemote >( *this, in, out );
                _lock.lock();
                if ( !async_retval.empty() )
                    return Done;
                break;
            case TAG_PARALLEL:
                _lock.unlock();
                rpc::demarshallWith< Instance, ParallelFromRemote >( *this, in, out );
                _lock.lock();
                break;
            case TAG_INTERRUPT:
                m_local.interrupt();
                break;
            default:
                ASSERT_UNREACHABLE_F( "unexpected tag %d", status.Get_tag() );
        }

        return Continue;
    }

    void interrupt() {
        std::unique_lock< std::mutex > _lock( mpi.global().mutex );
        mpi.notify( _lock, TAG_INTERRUPT, bitblock( m_mpiForwarder.pool ) );
        m_local.interrupt();
    }

};

};

}

namespace divine_test {

struct TestParallel {

    struct Counter {
        int i;
        void inc() { i++; }
        Counter() : i( 0 ) {}
    };

    TEST(runThread) {
        Counter c;
        ASSERT_EQ( c.i, 0 );
        RunThread< Counter > t( c, &Counter::inc );
        ASSERT_EQ( c.i, 0 );
        t.start();
        t.join();
        ASSERT_EQ( c.i, 1 );
    }

    TEST(threadVector) {
        std::vector< Counter > vec;
        vec.resize( 10 );
        ThreadVector< Counter > p( vec, &Counter::inc );
        p.run();

        for ( int i = 0; i < 10; ++i )
            ASSERT_EQ_IDX( i, vec[ i ].i, 1 );
    }

    struct ParallelCounter : Parallel< Topology< brick::types::Unit >::Local, ParallelCounter >
    {
        Counter counter;

        int get() { return counter.i; }
        void set( Counter c ) { counter = c; }
        void inc() { counter.inc(); }

        void run() {
            this->topology().distribute( counter, &ParallelCounter::set );
            this->topology().parallel( &ParallelCounter::inc );
        }

        ParallelCounter() {
            this->becomeMaster( 10 );
            this->initSlaves( *this );
        }

        ParallelCounter( ParallelCounter& m, std::pair< int, int > i ) {
            this->becomeSlave( m.topology(), i );
        }
    };

    template< typename X >
    void checkValues( X &x, int n, int k ) {
        std::vector< int > values;
        x.topology().collect( values, &X::get );
        ASSERT_EQ( values.size(), size_t( n ) );
        for ( size_t i = 0; i < values.size(); ++i )
            ASSERT_EQ_IDX( i, values[ i ], k );
    }

    TEST(parallel) {
        ParallelCounter c;
        ASSERT_EQ( c.get(), 0 );
        checkValues( c, 10, 0 );

        c.run();
        ASSERT_EQ( c.get(), 0 );
        checkValues( c, 10, 1 );
    }

    struct CommCounter : Parallel< Topology< int >::Local, CommCounter >
    {
        Counter counter;

        void tellInc() {
            submit( this->id(), (this->id() + 1) % this->peers(), 1 );
            do {
                if ( comms().pending( id() ) ) {
                    counter.i += comms().take( this->id() );
                    return;
                }
            } while ( true );
        }

        int get() { return counter.i; }
        void run() {
            this->topology().parallel( &CommCounter::tellInc );
        }

        CommCounter() {
            this->becomeMaster( 10 );
            this->initSlaves( *this );
            counter.i = 0;
        }

        CommCounter( CommCounter &m, std::pair< int, int > i ) {
            this->becomeSlave( m.topology(), i );
        }
    };

    TEST(comm) {
        CommCounter c;
        ASSERT_EQ( c.counter.i, 0 );
        checkValues( c, 10, 0 );

        c.run();
        ASSERT_EQ( c.counter.i, 0 );
        checkValues( c, 10, 1 );
    }
};

}

#endif
