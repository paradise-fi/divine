// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <wibble/test.h>
#include <divine/pool.h>
#include <divine/blob.h>
#include <divine/barrier.h>
#include <divine/fifo.h>

#include <divine/output.h>

#ifndef DIVINE_MPI_H
#define DIVINE_MPI_H

#ifdef HAVE_MPI
#include <mpi.h>
#endif

namespace divine {

#define TAG_ALL_DONE    0
#define TAG_RUN         1
#define TAG_GET_COUNTS  2
#define TAG_GIVE_COUNTS 4
#define TAG_DONE        5
#define TAG_SHARED      6
#define TAG_INTERRUPT   7
#define TAG_RING_RUN    8
#define TAG_RING_DONE   9
#define TAG_SOLICIT_SHARED 10

#define TAG_ID           100

namespace algorithm {

template< typename T >
struct _MpiId
{
    static int to_id( void (T::*f)() ) {
        // assert( 0 );
        return -1;
    }

    static void (T::*from_id( int ))() {
        // assert( 0 );
        return 0;
    }

    template< typename O >
    static void writeShared( typename T::Shared, O ) {
    }

    template< typename I >
    static I readShared( typename T::Shared &, I i ) {
        return i;
    }
};

}

enum Loop { Continue, Done };

#ifdef HAVE_MPI

struct MpiMonitor {
    // Called when a message with a matching tag has been received. The status
    // object for the message is handed in.
    virtual Loop process( wibble::sys::MutexLock &, MPI::Status & ) = 0;

    // Called whenever there is a pause in incoming traffic (i.e. a nonblocking
    // probe fails). A blocking probe will follow each call to progress().
    virtual Loop progress() { return Continue; }
};

struct MpiBase {
    wibble::sys::Mutex m_mutex;

    MpiMonitor * m_progress;
    std::vector< MpiMonitor * > m_monitor;

    bool is_master; // the master token
    bool m_initd;
    int m_rank, m_size;

    void send( const void *buf, int count, const MPI::Datatype &dt, int dest, int tag ) {
        wibble::sys::MutexLock _lock( m_mutex );
        send( _lock, buf, count, dt, dest, tag );
    }

    void send( wibble::sys::MutexLock &, const void *buf, int count,
               const MPI::Datatype &dt, int dest, int tag ) {
        MPI::COMM_WORLD.Send( buf, count, dt, dest, tag );
    }

    void registerProgress( MpiMonitor &m ) {
        m_progress = &m;
    }

    void registerMonitor( int tag, MpiMonitor &m ) {
        m_monitor.resize( std::max( size_t( tag ) + 1, m_monitor.size() ) );
        m_monitor[ tag ] = &m;
    }

    int rank() { if ( m_initd ) return m_rank; else return 0; }
    int size() { if ( m_initd ) return m_size; else return 1; }

    bool master() { return is_master; }

    MpiBase() : m_mutex( true ), m_progress( 0 ) {}

    std::ostream &mpidebug() {
        return Output::output().debug() << "MPI[" << rank() << "]: ";
    }
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
template< typename Algorithm, typename D >
struct Mpi : MpiBase {

    D *m_domain;
    typedef typename Algorithm::Shared Shared;
    Shared *master_shared;
    std::vector< Shared > m_shared;

    D &domain() {
        assert( m_domain );
        return *m_domain;
    }

    Mpi( Shared *sh, D *d ) : master_shared( sh )
    {
        m_domain = d;
        is_master = false;
        m_initd = false;
    }

    Shared &shared( int i ) {
        return m_shared[ i ];
    }

    void init() {
        if (m_initd)
            return;

        m_initd = true;
        MPI::Init();
        m_size = MPI::COMM_WORLD.Get_size();
        m_rank = MPI::COMM_WORLD.Get_rank();

        m_shared.resize( domain().peers() );

        domain().setupIds();

        if ( m_rank == 0 )
            is_master = true;
    }

    void start() {
        init();

        if ( !master() )
            while ( true )
                loop();
    }

    void notifyOne( int i, int tag, int id ) {
        MPI::COMM_WORLD.Send( &id, 1, MPI::INT, i, tag );
    }

    void notifySlaves( int tag, int id ) {
        if ( !master() )
            return;
        notify( tag, id );
    }

    void notify( int tag, int id ) {
        for ( int i = 0; i < size(); ++i ) {
            if ( i != rank() )
                notifyOne( i, tag, id );
        }
    }

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

        mpidebug() << "collecting shared bits" << std::endl;

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
        mpidebug() << "shared bits collected" << std::endl;
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
        mpidebug() << "sending shared bits..." << std::endl;
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
        while( loop() == Continue ); // wait (and serve) till the ring is done
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
        mpidebug() << "ALL DONE" << std::endl;
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
        domain().parallel().runInRing(
            *master_shared, algorithm::_MpiId< Algorithm >::from_id( id ) );
        _lock.reclaim();
        is_master = false;
        if ( rank() < size() - 1 )
            notifyOne( rank() + 1, TAG_RING_RUN, id );
        else
            notifyOne( 0, TAG_RING_DONE, 0 );
        sendSharedBits( ( rank() + 1 ) % size(), *master_shared );
    }

    void run( int id ) {
        Shared sh = obtainSharedBits();
        domain().parallel().run(
            sh, algorithm::_MpiId< Algorithm >::from_id( id ) );
    }

    Loop slave( wibble::sys::MutexLock &_lock, MPI::Status &status ) {
        assert_eq( status.Get_count( MPI::INT ), 1 );

        int id;

        mpidebug() << "slave( tag = " << status.Get_tag() << " )" << std::endl;

        MPI::COMM_WORLD.Recv( &id, 1, /* one integer per message */
                              MPI::INT,
                              MPI::ANY_SOURCE, /* anyone can become a master these days */
                              MPI::ANY_TAG, status );
        switch ( status.Get_tag() ) {
            case TAG_ALL_DONE:
                MPI::Finalize();
                exit( 0 ); // after a fashion...
            case TAG_RING_RUN:
                runSerial( _lock, id );
                break;
            case TAG_RING_DONE:
                assert( master_shared );
                *master_shared = obtainSharedBits();
                mpidebug() << "RING DONE" << std::endl;
                return Done;
            case TAG_RUN:
                _lock.drop();
                run( id );
                break;
            case TAG_SOLICIT_SHARED:
                returnSharedBits( status.Get_source() );
                break;
            default:
                assert( 0 );
        }

        return Continue;
    }

    Loop loop() {

        MPI::Status status;

        wibble::sys::MutexLock _lock( m_mutex );
        // And process incoming MPI traffic.
        while ( MPI::COMM_WORLD.Iprobe(
                    MPI::ANY_SOURCE, MPI::ANY_TAG, status ) )
        {
            int tag = status.Get_tag();
            if ( m_monitor.size() >= tag && m_monitor[ tag ] ) {
                if ( m_monitor[ tag ]->process( _lock, status ) == Done )
                    return Done;
            } else if ( slave( _lock, status ) == Done )
                return Done;
        }

        sched_yield();

        if ( m_progress )
            return m_progress->progress();

        /* wait for messages */
        /* MPI::COMM_WORLD.Probe(
            MPI::ANY_SOURCE, MPI::ANY_TAG ); */

        return Continue;
    }

};

/**
 * A high-level MPI bridge. This structure is designed to integrate into the
 * Domain framework (see parallel.h). The addExtra mechanism of Parallel can be
 * used to plug in the MpiWorker into a Parallel setup. The MpiWorker then
 * takes care to relay
 */
template< typename D >
struct MpiWorker: Terminable, MpiMonitor, wibble::sys::Thread {
    D &m_domain;
    int recv, sent;
    typedef typename D::Fifo Fifo;

    Pool pool;
    std::vector< Fifo > fifo;
    std::vector< std::vector< int32_t > > buffers;
    std::vector< std::pair< bool, MPI::Request > > requests;
    std::vector< int32_t > in_buffer;

    MpiWorker( D &d ) : m_domain( d ) {
        fifo.resize( d.peers() * d.peers() );
        buffers.resize( d.mpi.size() );
        requests.resize( d.mpi.size() );
        sent = recv = 0;
    }

    bool outgoingEmpty() {
        for ( int i = 0; i < fifo.size(); ++i )
            if ( !fifo[ i ].empty() )
                return false;
        return true;
    }

    bool isBusy() { return true; }

    bool workWaiting() {
        return !outgoingEmpty();
    }

    template< typename Mpi >
    std::pair< int, int > accumCounts( Mpi &mpi, int id ) {
        mpi.notifySlaves( TAG_GET_COUNTS, id );
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

    bool termination() {
        std::pair< int, int > one, two;

        one = accumCounts( m_domain.mpi, 0 );

        if ( one.first != one.second )
            return false;

        two = accumCounts( m_domain.mpi, 1 );

        if ( one.first == two.first && two.first == two.second ) {
            m_domain.mpi.notifySlaves( TAG_DONE, 0 );

            if ( m_domain.barrier().idle( this ) )
                return true;
        }

        return false;
    }

    void interrupt() {
        wibble::sys::MutexLock _lock( m_domain.mpi.m_mutex );
        mpidebug() << "INTERRUPTED (local)" << std::endl;
        m_domain.mpi.notify( TAG_INTERRUPT, 0 );
        sent += m_domain.mpi.size() - 1;
    }

    void receiveDataMessage( MPI::Status &status )
    {
        Blob b;
        int target;

        in_buffer.resize( status.Get_count( MPI::BYTE ) / 4 );
        MPI::COMM_WORLD.Recv( &in_buffer.front(), in_buffer.size() * 4,
                              MPI::BYTE,
                              status.Get_source(),
                              status.Get_tag(), status );
        std::vector< int32_t >::const_iterator i = in_buffer.begin();

        // FIXME. This here hardcoding of pairwise blob relationship is rather
        // evil, but necessary for correctness with regards to parallel visitor
        // implementation. It is however a modularity violation and this whole
        // pairing mess needs to be addressed somehow.
        while ( i != in_buffer.end() ) {
            target = *i++;
            assert_pred( m_domain.isLocalId, target );
            i = b.read32( &pool, i );
            m_domain.queue( -1, target ).push( b );
            i = b.read32( &pool, i );
            m_domain.queue( -1, target ).push( b );
        }
        ++ recv;
    }

    Loop process( wibble::sys::MutexLock &, MPI::Status &status ) {
        int id;
        bool ret;

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
            case TAG_DONE:
                mpidebug() << "DONE" << std::endl;
                return m_domain.barrier().idle( this ) ? Done : Continue;
            case TAG_INTERRUPT:
                mpidebug() << "INTERRUPTED (remote)" << std::endl;
                ++ recv;
                m_domain.interrupt( true );
                return Continue;
            default:
                assert_die();
        }
    }

    bool lastMan() {
        bool r = m_domain.barrier().lastMan( this );
        return r;
    }

    std::ostream &mpidebug() {
        return m_domain.mpi.mpidebug();
    }

    Loop progress() {
        // Fill outgoing buffers from the incoming FIFO queues...
        for ( int i = 0; i < fifo.size(); ++i ) {
            int to = i / m_domain.peers(),
              rank = to / m_domain.n;
            if ( m_domain.isLocalId( to ) ) {
                assert( fifo[ i ].empty() );
                continue;
            }

            // Build up linear buffers for MPI send. We only do that on
            // buffers that are not currently in-flight (request not busy).

            // FIXME the pairing of messages is necessary because we currently
            // do not maintain FIFO order for messages between a pair of
            // endpoints (threads)... when this is addressed, the limitation
            // here can be lifted
            while ( !fifo[ i ].empty() && !requests[ rank ].first &&
                    buffers[ rank ].size() < 100 * 1024)
            {
                buffers[ rank ].push_back( to );

                Blob b = fifo[ i ].front();
                fifo[ i ].pop();
                b.write32( std::back_inserter( buffers[ rank ] ) );
                b.free( pool );

                b = fifo[ i ].front( true );
                fifo[ i ].pop();
                b.write32( std::back_inserter( buffers[ rank ] ) );
                b.free( pool );
            }
        }

        // ... and flush the buffers.
        for ( int to = 0; to < buffers.size(); ++ to ) {
            if ( requests[ to ].first ) {
                if ( requests[ to ].second.Test() ) {
                    buffers[ to ].clear();
                    requests[ to ].first = false;
                }
                continue;
            }

            if ( buffers[ to ].empty() )
                continue;

            requests[ to ].first = true;
            requests[ to ].second = MPI::COMM_WORLD.Isend(
                    &buffers[ to ].front(),
                    buffers[ to ].size() * 4,
                    MPI::BYTE, to, TAG_ID );
            ++ sent;
        }

        // NB. The call to lastMan() here is first, since it needs to be done
        // by non-master nodes, to wake up sleeping workers that have received
        // messages from network. Ugly, yes.
        if ( lastMan() && m_domain.mpi.master() && outgoingEmpty() ) {
            if ( termination() ) {
                mpidebug() << "TERMINATED" << std::endl;
                return Done;
            }
        }

        return Continue;
    }

    void *main() {
        mpidebug() << "WORKER START" << std::endl;
        m_domain.barrier().started( this );
        m_domain.mpi.registerProgress( *this );
        m_domain.mpi.registerMonitor( TAG_ID, *this );
        m_domain.mpi.registerMonitor( TAG_GET_COUNTS, *this );
        m_domain.mpi.registerMonitor( TAG_DONE, *this );
        m_domain.mpi.registerMonitor( TAG_INTERRUPT, *this );

        while ( m_domain.mpi.loop() == Continue );

        mpidebug() << "WORKER DONE" << std::endl;
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

    void init() {}
    void start() {}
};

template< typename M, typename D >
struct Mpi : MpiBase {
    typename M::Shared &shared( int i ) {
        assert_die();
    }
    Mpi( typename M::Shared *, D * ) {}
};

template< typename D >
struct MpiWorker {
    struct Fifos {
        Fifo< Blob > &operator[]( int ) __attribute__((noreturn)) {
            assert_die();
        }
    };

    Fifos fifo;

    void interrupt() {}
    void start() {}

    MpiWorker( D& ) {}
};

#endif

}

#endif
