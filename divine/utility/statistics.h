// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#ifndef DIVINE_STATISTICS_H
#define DIVINE_STATISTICS_H

#include <memory>

#include <brick-shmem.h>
#include <divine/toolkit/mpi.h>
#include <divine/toolkit/pool.h>

#include <divine/utility/meta.h>
#include <divine/utility/sysinfo.h>

#define TAG_STATISTICS 128

namespace divine {

struct NoStatistics {
    void enqueue( int , int64_t ) {}
    void dequeue( int , int64_t ) {}
    void hashsize( int , int64_t ) {}
    void hashadded( int , int64_t ) {}
    void sent( int, int, int64_t ) {}
    void received( int, int, int64_t ) {}
    void idle( int ) {}
    void busy( int ) {}

    std::ostream *output;
    bool gnuplot;

    static NoStatistics _global;
    static NoStatistics &global() {
        return _global;
    }

    template< typename D >
    void useDomain( D & ) {}
    void start() {}
};

#ifndef __divine__

struct TrackStatistics : brick::shmem::Thread, MpiMonitor {
    struct PerThread {
        std::vector< int64_t > sent;
        std::vector< int64_t > received;
        int64_t enq, deq;
        int64_t hashsize;
        int64_t hashused;
        int64_t memQueue;
        int64_t memHashes;
        int64_t idle;
        int64_t cputime;
        std::vector< int64_t > memSent;
        std::vector< int64_t > memReceived;
    };

    std::vector< PerThread * > threads;
    divine::Mpi mpi;
    int pernode, localmin;

    bool gnuplot;
    std::ostream *output;
    int64_t memBaseline;

    void enqueue( int id , int64_t size ) {
        thread( id ).enq ++;
        thread( id ).memQueue += size;
    }

    void dequeue( int id , int64_t size ) {
        thread( id ).deq ++;
        thread( id ).memQueue -= size;
    }

    void hashsize( int id , int64_t s ) {
        thread( id ).hashsize = s;
    }

    void hashadded( int id , int64_t nodeSize ) {
        thread( id ).hashused ++;
        thread( id ).memHashes += nodeSize;
    }

    void idle( int id  ) {
        ++ thread( id ).idle;
    }

    void busy( int id );

    PerThread &thread( int id ) {
        ASSERT_LEQ( size_t( id ), threads.size() );
        if ( !threads[ id ] )
            threads[ id ] = new PerThread;
        return *threads[ id ];
    }

    void sent( int from, int to, int64_t nodeSize ) {
        ASSERT_LEQ( 0, to );

        PerThread &f = thread( from );
        if ( f.sent.size() <= size_t( to ) )
            f.sent.resize( to + 1, 0 );
        ++ f.sent[ to ];
        f.memSent[ to ] += nodeSize;
    }

    void received( int from, int to, int64_t nodeSize ) {
        ASSERT_LEQ( 0, from );

        PerThread &t = thread( to );
        if ( t.received.size() <= size_t( from ) )
            t.received.resize( from + 1, 0 );
        ++ t.received[ from ];
        t.memReceived[ from ] += nodeSize;
    }

    static int64_t first( int64_t a, int64_t ) { return a; }
    static int64_t second( int64_t, int64_t b ) { return b; }
    static int64_t diff( int64_t a, int64_t b ) { return a - b; }

    int64_t memUsed() {
        sysinfo::Info i;
        return i.peakVmSize() - memBaseline;
    }

    void resize( int s );
    template< typename F > void line( std::ostream &o, std::string lbl, F f );
    void matrix( std::ostream &o, int64_t (*what)(int64_t, int64_t) );
    void printv( std::ostream &o, int width, int64_t v, int64_t *sum );
    void label( std::ostream &o, std::string text, bool d = true );
    void format( std::ostream &o );
    void snapshot();
    void main();

    void send();
    Loop process( std::unique_lock< std::mutex > &, MpiStatus &status );

    void setup( const Meta &m );

    TrackStatistics() : pernode( 1 ), localmin( 0 )
    {
        output = 0;
        gnuplot = false;
        resize( 1 );
        sysinfo::Info i;
        memBaseline = i.peakVmSize();
    }

    ~TrackStatistics();

    static TrackStatistics &global() {
        static std::unique_ptr< TrackStatistics > g( new TrackStatistics );
        return *g;
    }
};

#endif // !__divine__

template <typename Ty>
int64_t memSize( Ty x, Pool& ) {
    return sizeof(x);
}

template <>
inline int64_t memSize<Blob>(Blob x, Pool& pool) {
    return sizeof( Blob ) + ( pool.valid( x ) ? align( pool.size( x ), sizeof( void * ) ) : 0 );
}


}

#endif
