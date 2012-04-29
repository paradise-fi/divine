// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <wibble/test.h>
#include <wibble/sys/thread.h>

#include <divine/pool.h>
#include <divine/blob.h>
#include <divine/barrier.h>
#include <divine/fifo.h>
#include <divine/meta.h>
#include <divine/rpc.h>

#include <divine/output.h>

#ifndef DIVINE_MPI_H
#define DIVINE_MPI_H

#ifdef O_MPI
#include <mpi.h>
#endif

namespace divine {

#define TAG_ALL_DONE    0
#define TAG_GET_COUNTS  1
#define TAG_GIVE_COUNTS 2
#define TAG_TERMINATED  3
#define TAG_INTERRUPT   4

#define TAG_PARALLEL    5
#define TAG_RING        6
#define TAG_COLLECT     7

#define TAG_ID        100

enum Loop { Continue, Done };

#ifdef O_MPI

struct MpiMonitor {
    // Called when a message with a matching tag has been received. The status
    // object for the message is handed in.
    virtual Loop process( wibble::sys::MutexLock &, MPI::Status & ) = 0;

    // Called whenever there is a pause in incoming traffic (i.e. a nonblocking
    // probe fails). A blocking probe will follow each call to progress().
    virtual Loop progress( wibble::sys::MutexLock & ) { return Continue; }

    virtual ~MpiMonitor() {}
};

/**
 * A global per-mpi-node structure that manages the low-level aspects of MPI
 * communication. Each node should call start() when it is ready to do so
 * (i.e. the local data structures are set up and the algorithm is about to be
 * started). On the master node, start() returns control to its caller after
 * setting up MPI. However, on slave (non-master) nodes, it seizes the control
 * and never returns. The slave nodes then wait for commands from the master.
 *
 * The MpiWorker (see below) is designed to plug into the Domain framework (see
 * parallel.h) and to relay the parallel-start and the termination detection
 * over MPI to the slave nodes.
 */
struct Mpi {

private:
    struct Data {
        wibble::sys::Mutex mutex;

        MpiMonitor * progress;
        std::vector< MpiMonitor * > monitor;

        bool is_master; // the master token
        int rank, size;
        int instances;
    };

    static Data *s_data;

public:
    Data &global() {
        return *s_data;
    }

    void send( const void *buf, int count, const MPI::Datatype &dt, int dest, int tag ) {
        wibble::sys::MutexLock _lock( global().mutex );
        send( _lock, buf, count, dt, dest, tag );
    }

    void send( wibble::sys::MutexLock &, const void *buf, int count,
               const MPI::Datatype &dt, int dest, int tag ) {
        MPI::COMM_WORLD.Send( buf, count, dt, dest, tag );
    }

    void registerProgress( MpiMonitor &m ) {
        global().progress = &m;
    }

    void registerMonitor( int tag, MpiMonitor &m ) {
        global().monitor.resize( std::max( size_t( tag ) + 1, global().monitor.size() ) );
        global().monitor[ tag ] = &m;
    }

    int rank() { return global().rank; }
    int size() { return global().size; }

    bool master() { return global().is_master; }

    std::ostream &debug() {
        return Output::output().debug() << "MPI[" << rank() << "]: ";
    }

    Mpi();
    Mpi( const Mpi & );
    ~Mpi();

    void start() {
        if ( !master() )
            while ( true )
                loop();
    }

    rpc::bitstream &recvStream( wibble::sys::MutexLock &_lock,
                                MPI::Status &st, rpc::bitstream &bs )
    {
        MPI::COMM_WORLD.Probe( st.Get_source(), st.Get_tag(), st );
        int first = bs.bits.size(), count = st.Get_count( MPI::BYTE ) / 4;
        bs.bits.resize( first + count );
        MPI::COMM_WORLD.Recv( &bs.bits[ first ], count * 4,
                              MPI::BYTE, st.Get_source(), st.Get_tag(), st );
        debug() << "got (tag = " << st.Get_tag() << ", src = " << st.Get_source() << "): "
                << wibble::str::fmt( bs.bits ) << std::endl;
        return bs;
    }

    rpc::bitstream &sendStream( wibble::sys::MutexLock &_lock,
                                rpc::bitstream &bs, int to, int tag )
    {
        debug() << "sending (tag = " << tag << ", to = " << to << "): "
                << wibble::str::fmt( bs.bits ) << std::endl;
        MPI::COMM_WORLD.Send( &bs.bits.front(),
                              bs.bits.size() * 4,
                              MPI::BYTE, to, tag );
        return bs;
    }

    void notifyOne( wibble::sys::MutexLock &_lock,
                    int i, int tag, rpc::bitstream bs = rpc::bitstream() )
    {
        sendStream( _lock, bs, i, tag );
    }

    void notifySlaves( wibble::sys::MutexLock &_lock,
                       int tag, rpc::bitstream bs = rpc::bitstream() )
    {
        if ( !master() )
            return;
        notify( _lock, tag, bs );
    }

    void notify( wibble::sys::MutexLock &_lock,
                 int tag, rpc::bitstream bs = rpc::bitstream() )
    {
        for ( int i = 0; i < size(); ++i ) {
            if ( i != rank() )
                notifyOne( _lock, i, tag, bs );
        }
    }

    Loop loop() {

        MPI::Status status;

        wibble::sys::MutexLock _lock( global().mutex );
        // And process incoming MPI traffic.
        while ( MPI::COMM_WORLD.Iprobe(
                    MPI::ANY_SOURCE, MPI::ANY_TAG, status ) )
        {
            int tag = status.Get_tag();

            if ( tag == TAG_ALL_DONE ) {
                MPI::Finalize();
                exit( 0 ); // after a fashion...
            }

            if ( global().monitor.size() >= size_t( tag ) && global().monitor[ tag ] ) {
                if ( global().monitor[ tag ]->process( _lock, status ) == Done )
                    return Done;
            } else assert_die();
        }

        sched_yield();

        if ( global().progress )
            return global().progress->progress( _lock );

        /* wait for messages */
        /* MPI::COMM_WORLD.Probe(
            MPI::ANY_SOURCE, MPI::ANY_TAG ); */

        return Continue;
    }
#if 0
    void returnSharedBits( int to ) {
        std::vector< int32_t > shbits;
        for ( int i = 0; i < domain().n; ++i ) {
            algorithm::_MpiId< Algorithm >::writeShared(
                domain().parallel().shared( i ),
                std::back_inserter( shbits ) );
        }
        MPI::COMM_WORLD.Send( &shbits.front(),
                              shbits.size() * 4,
                              MPI::BYTE, to, TAG_SHARED );
    }

    void collectSharedBits() {
        if (!master())
            return;

        wibble::sys::MutexLock _lock( m_mutex );

        MPI::Status status;

        debug() << "collecting shared bits" << std::endl;

        std::vector< int32_t > shbits;
        notifySlaves( TAG_SOLICIT_SHARED, 0 );
        for ( int i = 0; i < size(); ++ i ) {
            if ( i == rank() )
                continue;
            MPI::COMM_WORLD.Probe( i, TAG_SHARED, status );
            shbits.resize( status.Get_count( MPI::BYTE ) / 4 );
            MPI::COMM_WORLD.Recv( &shbits.front(), shbits.size() * 4,
                                  MPI::BYTE, i, TAG_SHARED, status );
            std::vector< int32_t >::const_iterator it = shbits.begin();
            for ( int k = 0; k < domain().n; ++k ) {
                it = algorithm::_MpiId< Algorithm >::readShared(
                    m_shared[ i * domain().n + k ], it );
            }
            assert( it == shbits.end() ); // sanity check
        }
        debug() << "shared bits collected" << std::endl;
    }

    Shared obtainSharedBits() {
        MPI::Status status;
        std::vector< int32_t > shbits;
        Shared sh = *master_shared;

        MPI::COMM_WORLD.Probe( MPI::ANY_SOURCE, TAG_SHARED, status );
        shbits.resize( status.Get_count( MPI::BYTE ) / 4 );
        MPI::COMM_WORLD.Recv( &shbits.front(), shbits.size() * 4,
                              MPI::BYTE, MPI::ANY_SOURCE, TAG_SHARED, status );
        algorithm::_MpiId< Algorithm >::readShared( sh, shbits.begin() );
        return sh;
    }

    void sendSharedBits( int to, const Shared &sh ) {
        std::vector< int32_t > shbits;
        algorithm::_MpiId< Algorithm >::writeShared(
            sh, std::back_inserter( shbits ) );
        debug() << "sending shared bits to " << to << "..." << std::endl;
        MPI::COMM_WORLD.Send( &shbits.front(),
                              shbits.size() * 4,
                              MPI::BYTE, to, TAG_SHARED );
    }

    template< typename F >
    void runInRing( F f ) {
        if ( rank() != 0 ) return;
        if ( size() <= 1 ) return;

        wibble::sys::MutexLock _lock( m_mutex );

        is_master = false;
        notifyOne( 1, TAG_RING_RUN,
                   algorithm::_MpiId< Algorithm >::to_id( f ) );
        sendSharedBits( 1, *master_shared );

        _lock.drop();
        while ( loop() == Continue ) ; // wait (and serve) till the ring is done
        is_master = true;
    }

    template< typename Shared, typename F >
    void runOnSlaves( Shared sh, F f ) {
        if ( !master() ) return;
        if ( size() <= 1 ) return; // master_shared can be NULL otherwise

        wibble::sys::MutexLock _lock( m_mutex );

        notifySlaves( TAG_RUN,
                      algorithm::_MpiId< Algorithm >::to_id( f ) );
        for ( int i = 0; i < size(); ++i ) {
            if ( i != rank() )
                sendSharedBits( i, sh );
        }
    }

    ~Mpi() {
        debug() << "ALL DONE" << std::endl;
        if ( m_initd && master() ) {
            notifySlaves( TAG_ALL_DONE, 0 );
            MPI::Finalize();
        }
    }

    // Default copy and assignment is fine for us.

    void runSerial( wibble::sys::MutexLock &_lock, int id ) {
        is_master = true;
        *master_shared = obtainSharedBits();
        _lock.drop();
        debug() << "runSerial " << id << "..." << std::endl;
        domain().parallel().runInRing(
            *master_shared, algorithm::_MpiId< Algorithm >::from_id( id ) );
        debug() << "runSerial " << id << " locally done..." << std::endl;
        _lock.reclaim();
        is_master = false;
        if ( rank() < size() - 1 ) {
            debug() << "passing control to " << rank() + 1 << "..." << std::endl;
            notifyOne( rank() + 1, TAG_RING_RUN, id );
        } else {
            debug() << "ring finished, passing control back to master..." << std::endl;
            notifyOne( 0, TAG_RING_DONE, 0 );
        }
        sendSharedBits( ( rank() + 1 ) % size(), *master_shared );
    }

    void run( int id ) {
        Shared sh = obtainSharedBits();
        domain().parallel().run(
            sh, algorithm::_MpiId< Algorithm >::from_id( id ) );
    }
#endif

};

template< typename T >
struct Matrix {
    std::vector< std::vector< T > > matrix;

    void resize( int x, int y ) {
        matrix.resize( x );
        for ( int i = 0; i < x; ++i )
            matrix[ i ].resize( y );
    }

    std::vector< T > &operator[]( int i ) {
        return matrix[ i ];
    }
};

template< typename T > struct FifoMatrix;

/**
 * A high-level MPI bridge. This structure is designed to integrate into the
 * Domain framework (see parallel.h). The addExtra mechanism of Parallel can be
 * used to plug in the MpiWorker into a Parallel setup. The MpiWorker then
 * takes care to relay
 */
template< typename Comms = FifoMatrix< Blob > >
struct MpiForwarder : Terminable, MpiMonitor, wibble::sys::Thread {
    int recv, sent;

    Pool pool;
    Matrix< std::vector< int32_t > > buffers;
    Matrix< std::pair< bool, MPI::Request > >  requests;
    std::vector< int32_t > in_buffer;

    Comms *m_comms;
    Barrier< Terminable > *m_barrier;
    Mpi mpi;
    int m_peers;
    int m_localMin, m_localMax;

    Comms &comms() {
        assert( m_comms );
        return *m_comms;
    }

    bool isLocal( int id ) {
        return id >= m_localMin && id <= m_localMax;
    }

    int rankOf( int id ) {
        return id / (m_localMax - m_localMin + 1);
    }

    MpiForwarder( Barrier< Terminable > *barrier, Comms *comms, int total, int min, int max )
        : m_comms( comms ), m_barrier( barrier )
    {
        buffers.resize( total, total );
        requests.resize( total, total );
        sent = recv = 0;
        m_peers = total;
        m_localMin = min;
        m_localMax = max;
    }

    bool outgoingEmpty() {
        for ( int from = 0; from < m_peers; ++from )
            for ( int to = 0; to < m_peers; ++to ) {
                if ( !isLocal( from ) || isLocal( to ) )
                    continue;

                if ( comms().pending( from, to ) )
                    return false;
            }
        return true;
    }

    bool isBusy() { return true; }

    bool workWaiting() {
        return !outgoingEmpty();
    }

    std::pair< int, int > accumCounts( wibble::sys::MutexLock &_lock, int id ) {
        rpc::bitstream bs;
        bs << id;

        mpi.notifySlaves( _lock, TAG_GET_COUNTS, bs );
        MPI::Status status;
        int r = recv, s = sent;
        bool valid = true;
        for ( int i = 0; i < mpi.size(); ++ i ) {

            if ( i == mpi.rank() )
                continue;

            int cnt[3];
            MPI::COMM_WORLD.Recv( cnt, 3, MPI::INT,
                                  MPI::ANY_SOURCE, TAG_GIVE_COUNTS,
                                  status );
            if ( !cnt[0] )
                valid = false;
            s += cnt[1];
            r += cnt[2];
        }
        return valid ? std::make_pair( r, s ) : std::make_pair( 0, 1 );
    }

    bool termination( wibble::sys::MutexLock &_lock ) {
        std::pair< int, int > one, two;

        one = accumCounts( _lock, 0 );

        if ( one.first != one.second )
            return false;

        two = accumCounts( _lock, 1 );

        if ( one.first == two.first && two.first == two.second ) {
            mpi.notifySlaves( _lock, TAG_TERMINATED );

            if ( m_barrier->idle( this ) )
                return true;
        }

        return false;
    }

    void receiveDataMessage( MPI::Status &status )
    {
        typename Comms::T b;

        in_buffer.resize( status.Get_count( MPI::BYTE ) / 4 );
        MPI::COMM_WORLD.Recv( &in_buffer.front(), in_buffer.size() * 4,
                              MPI::BYTE,
                              status.Get_source(),
                              status.Get_tag(), status );
        std::vector< int32_t >::const_iterator i = in_buffer.begin();

        int from = *i++;
        int to = *i++;
        // mpi.debug() << "DATA: " << from << " -> " << to << std::endl;
        assert_pred( isLocal, to );

        while ( i != in_buffer.end() ) {
            i = b.first.read32( &pool, i );
            i = b.second.read32( &pool, i );
            comms().submit( from, to, b );
        }
        ++ recv;
    }

    Loop process( wibble::sys::MutexLock &, MPI::Status &status ) {
        int id;

        if ( status.Get_tag() == TAG_ID ) {
            receiveDataMessage( status );
            return Continue;
        }

        MPI::COMM_WORLD.Recv( &id, 1, MPI::INT,
                              status.Get_source(),
                              status.Get_tag(), status );
        switch ( status.Get_tag() ) {
            case TAG_GET_COUNTS:
                int cnt[3];
                cnt[0] = outgoingEmpty() && lastMan();
                cnt[1] = sent;
                cnt[2] = recv;
                MPI::COMM_WORLD.Send( cnt, 3, MPI::INT,
                                      status.Get_source(), TAG_GIVE_COUNTS );
                return Continue;
            case TAG_TERMINATED:
                mpi.debug() << "DONE" << std::endl;
                return m_barrier->idle( this ) ? Done : Continue;
            default:
                assert_die();
        }
    }

    bool lastMan() {
        bool r = m_barrier->lastMan( this );
        return r;
    }

    Loop progress( wibble::sys::MutexLock &_lock ) {
        // Fill outgoing buffers from the incoming FIFO queues...
        for ( int from = 0; from < m_peers; ++from ) {
            for ( int to = 0; to < m_peers; ++to ) {

                if ( !isLocal( from ) || isLocal( to ) )
                    continue;

                /* initialise the buffer with from/to information */
                if ( buffers[ from ][ to ].empty() ) {
                    buffers[ from ][ to ].push_back( from );
                    buffers[ from ][ to ].push_back( to );
                }

                // Build up linear buffers for MPI send. We only do that on
                // buffers that are not currently in-flight (request not busy).
                while ( comms().pending( from, to ) && !requests[ from ][ to ].first &&
                        buffers[ from ][ to ].size() < 100 * 1024 )
                {
                    typename Comms::T b = comms().take( from, to );
                    b.first.write32( std::back_inserter( buffers[ from ][ to ] ) );
                    b.first.free( pool );
                    b.second.write32( std::back_inserter( buffers[ from ][ to ] ) );
                    b.second.free( pool );
                }
            }
        }

        // ... and flush the buffers.
        for ( int from = 0; from < m_peers; ++from ) {
            for ( int to = 0; to < m_peers; ++to ) {

                if ( !isLocal( from ) || isLocal( to ) )
                    continue;

                if ( requests[ from ][ to ].first ) {
                    if ( requests[ from ][ to ].second.Test() ) {
                        buffers[ from ][ to ].clear();
                        requests[ from ][ to ].first = false;
                    }
                    continue;
                }

                /* the first 2 elements are the from/to designation */
                if ( buffers[ from ][ to ].size() <= 2 )
                    continue;

                /* mpi.debug() << "SENDING: " << from << " -> " << to << std::endl;
                mpi.debug() << "BUFFER: " << buffers[ from ][ to ][ 0 ] << ", "
                << buffers[ from ][ to ][ 1 ] << " ... " << std::endl; */

                requests[ from ][ to ].first = true;
                requests[ from ][ to ].second = MPI::COMM_WORLD.Isend(
                    &buffers[ from ][ to ].front(),
                    buffers[ from ][ to ].size() * 4,
                    MPI::BYTE, rankOf( to ), TAG_ID );
                ++ sent;
            }
        }

        // NB. The call to lastMan() here is first, since it needs to be done
        // by non-master nodes, to wake up sleeping workers that have received
        // messages from network. Ugly, yes.
        if ( lastMan() && mpi.master() && outgoingEmpty() ) {
            if ( termination( _lock ) ) {
                mpi.debug() << "TERMINATED" << std::endl;
                return Done;
            }
        }

        return Continue;
    }

    void *main() {
        mpi.debug() << "FORWARDER START" << std::endl;
        m_barrier->started( this );
        mpi.registerProgress( *this );
        mpi.registerMonitor( TAG_ID, *this );
        mpi.registerMonitor( TAG_GET_COUNTS, *this );
        mpi.registerMonitor( TAG_TERMINATED, *this );

        while ( mpi.loop() == Continue ) ;

        mpi.debug() << "FORWARDER DONE" << std::endl;
        return 0;
    }
};

#else

struct MpiMonitor {};

struct MpiBase {
    int rank() { return 0; }
    int size() { return 1; }
    bool master() { return true; }
    void notifySlaves( int, int ) {}
    void collectSharedBits() {}

    void registerProgress( MpiMonitor & ) {}
    void registerMonitor( int, MpiMonitor & ) {}

    template< typename Shared, typename F >
    void runOnSlaves( Shared, F ) {}

    template< typename F >
    void runInRing( F f ) {}

    virtual void init( Meta ) {}
    void start() {}
};

template< typename M, typename D >
struct Mpi : MpiBase {
    typename M::Shared &shared( int i ) {
        assert_die();
    }
    Mpi( typename M::Shared *, D * ) {}
};

template< typename D, typename C >
struct MpiWorker {
    void interrupt() {}
    void start() {}
    void* join() { return 0; }

    MpiWorker( D& ) {}
};

#endif

}

#endif
