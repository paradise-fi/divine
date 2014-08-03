// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <mutex>

#include <brick-shmem.h>
#include <wibble/param.h>

#include <divine/toolkit/pool.h>
#include <divine/toolkit/barrier.h>
#include <divine/toolkit/bitstream.h>

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
typedef MPI::Status MpiStatus;
typedef MPI::Request MpiRequest;
#else
struct MpiRequest {
    bool Test() { return true; }
};
struct MpiStatus {
    int Get_tag() { return 0; }
    int Get_source() { return 0; }
};
#endif


struct MpiMonitor {
    // Called when a message with a matching tag has been received. The status
    // object for the message is handed in.
    virtual Loop process( std::unique_lock< std::mutex > &, MpiStatus & ) = 0;

    // Called whenever there is a pause in incoming traffic (i.e. a nonblocking
    // probe fails). A blocking probe will follow each call to progress().
    virtual Loop progress( std::unique_lock< std::mutex > & ) { return Continue; }

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
        std::mutex mutex;

        MpiMonitor * progress;
        std::vector< MpiMonitor * > monitor;

        bool is_master; // the master token
        int rank, size;
        int instances;

        bool isMpi; // running with MPI enabled
    };

    static Data *s_data;

public:
    typedef MpiStatus Status;

#ifdef O_MPI
    static const int anySource = MPI::ANY_SOURCE;
    static const int anyTag = MPI::ANY_TAG;
#else
    static const int anySource = 42;
    static const int anyTag = 42;
#endif

    Data &global() {
        return *s_data;
    }

    void send( const void *buf, int count, int dest, int tag ) {
        std::unique_lock< std::mutex > _lock( global().mutex );
        send( _lock, buf, count, dest, tag );
    }

    void send( std::unique_lock< std::mutex > &, const void *buf, int count, int dest, int tag ) {
#ifdef O_MPI
            return MPI::COMM_WORLD.Send( buf, count, MPI::BYTE, dest, tag );
#else
            wibble::param::discard( buf, count, dest, tag );
#endif
    }

    MpiRequest isend( std::unique_lock< std::mutex > &, const void *buf, int count, int dest, int tag ) {
#ifdef O_MPI
        return MPI::COMM_WORLD.Isend( buf, count, MPI::BYTE, dest, tag );
#else
        return MpiRequest();
        wibble::param::discard( buf, count, dest, tag );
#endif
    }

    void recv( void *mem, size_t count, int source, int tag, Status &st ) {
#ifdef O_MPI
        MPI::COMM_WORLD.Recv( mem, count, MPI::BYTE, source, tag, st );
#else
        wibble::param::discard( mem, count, source, tag, st );
#endif
    }

    bool probe( int source, int tag, Status &st, bool wait = true ) {
#ifdef O_MPI
        if (wait) {
            MPI::COMM_WORLD.Probe( source, tag, st );
            return false;
        } else
            return MPI::COMM_WORLD.Iprobe( source, tag, st );
#endif
        wibble::param::discard( source, tag, st, wait );
        return false;
    }

    size_t size( const Status &st ) {
#ifdef O_MPI
        return st.Get_count( MPI::BYTE );
#endif
        wibble::param::discard( st );
        return 0;
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

    Mpi( bool forceMpi = false );
    Mpi( const Mpi & );
    ~Mpi();

    void start() {
        if ( !master() )
            while ( true )
                loop();
    }

    bitblock &getStream( std::unique_lock< std::mutex > &_lock, int source, int tag, bitblock &bs )
    {
        Status st;
        probe( source, tag, st );
        return recvStream( _lock, st, bs );
    }

    bitblock &recvStream( std::unique_lock< std::mutex > &, Status &st, bitblock &bs )
    {
        probe( st.Get_source(), st.Get_tag(), st );
        int first = bs.bits.size(), count = size( st ) / 4;
        bs.bits.resize( first + count );
        recv( &bs.bits[ first ], count * 4, st.Get_source(), st.Get_tag(), st );
        return bs;
    }

    bitblock &sendStream( std::unique_lock< std::mutex > &_lock, bitblock &bs, int to, int tag )
    {
        void *data = &*(bs.bits.begin() + bs.offset);
        send( _lock, data, bs.size() * 4, to, tag );
        return bs;
    }

    void notifySlaves( std::unique_lock< std::mutex > &_lock,
                       int tag, bitblock bs )
    {
        if ( !master() )
            return;
        notify( _lock, tag, bs );
    }

    void notify( std::unique_lock< std::mutex > &_lock,
                 int tag, bitblock bs )
    {
        for ( int i = 0; i < size(); ++i ) {
            if ( i != rank() )
                sendStream( _lock, bs, i, tag );
        }
    }

    Loop loop() {

        Status status;

        std::unique_lock< std::mutex > _lock( global().mutex );
        // And process incoming MPI traffic.
        while ( probe( anySource, anyTag, status, false ) )
        {
            int tag = status.Get_tag();

            if ( tag == TAG_ALL_DONE ) {
#ifdef O_MPI
                MPI::Finalize();
#endif
                exit( 0 ); // after a fashion...
            }

            if ( global().monitor.size() >= size_t( tag ) && global().monitor[ tag ] ) {
                if ( global().monitor[ tag ]->process( _lock, status ) == Done )
                    return Done;
            } else assert_die();
        }

#ifdef POSIX
        sched_yield();
#endif

        if ( global().progress )
            return global().progress->progress( _lock );

        /* wait for messages */
        /* MPI::COMM_WORLD.Probe(
            MPI::ANY_SOURCE, MPI::ANY_TAG ); */

        return Continue;
    }

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
struct MpiForwarder : Terminable, MpiMonitor, brick::shmem::Thread {
    int recv, sent;

    Pool pool;
    Matrix< bitblock > buffers;
    Matrix< std::pair< bool, MpiRequest > >  requests;
    bitblock in_buffer;

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

    MpiForwarder( const Pool &p, Barrier< Terminable > *barrier, Comms *comms, int total, int min, int max )
        : pool( p ), m_comms( comms ), m_barrier( barrier )
    {
        buffers.resize( total, total );
        for ( int i = 0; i < total; ++i ) {
            for ( int j = 0; j < total; ++j ) {
                buffers[ i ][ j ].pool = &pool;
            }
        }
        in_buffer.pool = &pool;
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

    std::pair< int, int > accumCounts( std::unique_lock< std::mutex > &_lock, int id ) {
        bitblock bs( pool );
        bs << id;

        mpi.notifySlaves( _lock, TAG_GET_COUNTS, bs );
        int r = recv, s = sent;
        bool valid = true;
        for ( int i = 0; i < mpi.size(); ++ i ) {

            if ( i == mpi.rank() )
                continue;

            bitblock in( pool );
            mpi.getStream( _lock, mpi.anySource, TAG_GIVE_COUNTS, in );

            int addr, adds;
            in >> valid >> adds >> addr;
            s += adds;
            r += addr;
        }
        return valid ? std::make_pair( r, s ) : std::make_pair( 0, 1 );
    }

    bool termination( std::unique_lock< std::mutex > &_lock ) {
        std::pair< int, int > one, two;

        one = accumCounts( _lock, 0 );

        if ( one.first != one.second )
            return false;

        two = accumCounts( _lock, 1 );

        if ( one.first == two.first && two.first == two.second ) {
            mpi.notifySlaves( _lock, TAG_TERMINATED, bitblock( pool ) );

            if ( m_barrier->idle( this ) )
                return true;
        }

        return false;
    }

    void receiveDataMessage( std::unique_lock< std::mutex >& _lock, MpiStatus &status )
    {
        typename Comms::T b;

        mpi.recvStream( _lock, status, in_buffer );

        int from, to;
        in_buffer >> from >> to;
        assert_pred( isLocal, to );

        while ( !in_buffer.empty() ) {
            in_buffer >> b;
            comms().submit( from, to, b );
        }
        ++ recv;
    }

    Loop process( std::unique_lock< std::mutex > &_lock, MpiStatus &status ) {

        if ( status.Get_tag() == TAG_ID ) {
            receiveDataMessage( _lock, status );
            return Continue;
        }

        bitblock in( pool ), out( pool );
        mpi.recvStream( _lock, status, in );

        switch ( status.Get_tag() ) {
            case TAG_GET_COUNTS:
                out << (outgoingEmpty() && lastMan()) << sent << recv;
                mpi.sendStream( _lock, out, status.Get_source(), TAG_GIVE_COUNTS );
                return Continue;
            case TAG_TERMINATED:
                return m_barrier->idle( this ) ? Done : Continue;
            default:
                assert_die();
        }
    }

    bool lastMan() {
        bool r = m_barrier->lastMan( this );
        return r;
    }

    Loop progress( std::unique_lock< std::mutex > &_lock ) {
        // Fill outgoing buffers from the incoming FIFO queues...
        for ( int from = 0; from < m_peers; ++from ) {
            for ( int to = 0; to < m_peers; ++to ) {

                if ( !isLocal( from ) || isLocal( to ) )
                    continue;

                /* initialise the buffer with from/to information */
                if ( buffers[ from ][ to ].empty() )
                    buffers[ from ][ to ] << from << to;

                // Build up linear buffers for MPI send. We only do that on
                // buffers that are not currently in-flight (request not busy).
                while ( comms().pending( from, to ) && !requests[ from ][ to ].first &&
                        buffers[ from ][ to ].size() < 100 * 1024 )
                {
                    typename Comms::T b = comms().take( from, to );
                    std::get< 0 >( b ).setPool( pool );
                    buffers[ from ][ to ] << b;
                    pool.free( std::get< 1 >( b ) );
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

                requests[ from ][ to ].first = true;
                requests[ from ][ to ].second = mpi.isend( _lock,
                    &buffers[ from ][ to ].front(),
                    buffers[ from ][ to ].size() * 4,
                    rankOf( to ), TAG_ID );
                ++ sent;
            }
        }

        // NB. The call to lastMan() here is first, since it needs to be done
        // by non-master nodes, to wake up sleeping workers that have received
        // messages from network. Ugly, yes.
        if ( lastMan() && mpi.master() && outgoingEmpty() ) {
            if ( termination( _lock ) )
                return Done;
        }

        return Continue;
    }

    void main() {
        m_barrier->started( this );
        mpi.registerProgress( *this );
        mpi.registerMonitor( TAG_ID, *this );
        mpi.registerMonitor( TAG_GET_COUNTS, *this );
        mpi.registerMonitor( TAG_TERMINATED, *this );

        while ( mpi.loop() == Continue ) ;
    }
};

}

#endif
