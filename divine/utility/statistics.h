// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#ifndef DIVINE_STATISTICS_H
#define DIVINE_STATISTICS_H

#include <wibble/sys/thread.h>
#include <wibble/sys/mutex.h>
#include <wibble/regexp.h>
#include <divine/toolkit/mpi.h>

#include <divine/utility/meta.h>
#include <divine/utility/sysinfo.h>

#define TAG_STATISTICS 128

namespace divine {

struct NoStatistics {
    void enqueue( int , int ) {}
    void dequeue( int , int ) {}
    void hashsize( int , int ) {}
    void hashadded( int , int ) {}
    void sent( int, int, int ) {}
    void received( int, int, int ) {}

    std::ostream *output;
    bool gnuplot;

    static NoStatistics &global() {
        static NoStatistics *g = new NoStatistics;
        return *g;
    }

    template< typename D >
    void useDomain( D &d ) {}
    void start() {}
};

struct Statistics : wibble::sys::Thread, MpiMonitor {
    struct PerThread {
        std::vector< int64_t > sent;
        std::vector< int64_t > received;
        int64_t enq, deq;
        int64_t hashsize;
        int64_t hashused;
        int64_t memQueue;
        int64_t memHashes;
        std::vector< int64_t > memSent;
        std::vector< int64_t > memReceived;
    };

    std::vector< PerThread * > threads;
    divine::Mpi mpi;
    int pernode, localmin;

    bool gnuplot;
    std::ostream *output;
    int memBaseline;

    void enqueue( int id , int size ) {
        thread( id ).enq ++;
        thread( id ).memQueue += size;
    }

    void dequeue( int id , int size ) {
        thread( id ).deq ++;
        thread( id ).memQueue -= size;
    }

    void hashsize( int id , int s ) {
        thread( id ).hashsize = s;
    }

    void hashadded( int id , int nodeSize ) {
        thread( id ).hashused ++;
        thread( id ).memHashes += nodeSize;
    }

    PerThread &thread( int id ) {
        assert_leq( size_t( id ), threads.size() );
        if ( !threads[ id ] )
            threads[ id ] = new PerThread;
        return *threads[ id ];
    }

    void sent( int from, int to, int nodeSize ) {
        assert_leq( 0, to );

        PerThread &f = thread( from );
        if ( f.sent.size() <= size_t( to ) )
            f.sent.resize( to + 1, 0 );
        ++ f.sent[ to ];
        f.memSent[ to ] += nodeSize;
    }

    void received( int from, int to, int nodeSize ) {
        assert_leq( 0, from );

        PerThread &t = thread( to );
        if ( t.received.size() <= size_t( from ) )
            t.received.resize( from + 1, 0 );
        ++ t.received[ from ];
        t.memReceived[ from ] += nodeSize;
    }

    static int first( int a, int ) { return a; }
    static int second( int, int b ) { return b; }
    static int diff( int a, int b ) { return a - b; }

    int memUsed() {
        sysinfo::Info i;
        return i.peakVmSize() - memBaseline;
    }

    void resize( int s );
    void matrix( std::ostream &o, int (*what)(int, int) );
    void printv( std::ostream &o, int width, int v, int *sum );
    void label( std::ostream &o, std::string text, bool d = true );
    void format( std::ostream &o );
    void snapshot();
    void *main();

    void send();
    Loop process( wibble::sys::MutexLock &, MpiStatus &status );

    void setup( const Meta &m );

    Statistics() : pernode( 1 ), localmin( 0 )
    {
        output = 0;
        gnuplot = false;
        resize( 1 );
        sysinfo::Info i;
        memBaseline = i.peakVmSize();
    }

    static Statistics &global() {
        static Statistics *g = new Statistics;
        return *g;
    }
};

template <typename Ty>
int memSize(Ty x) {
    return sizeof(x);
}

template <>
inline int memSize<Blob>(Blob x) {
    return sizeof(Blob) + (x.valid() ? Blob::allocationSize(x.size()) : 0);
}

}

#endif
