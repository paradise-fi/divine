// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <wibble/sys/thread.h>
#include <wibble/sys/mutex.h>
#include <wibble/string.h>
#include <divine/mpi.h>
#include <iomanip>

#ifndef DIVINE_STATISTICS_H
#define DIVINE_STATISTICS_H

#define TAG_STATISTICS 128

namespace divine {

struct NoStatistics {
    void enqueue( int ) {}
    void dequeue( int ) {}
    void hashsize( int, int ) {}
    void hashadded( int ) {}
    void sent( int, int ) {}
    void received( int, int ) {}

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
        std::vector< int > sent;
        std::vector< int > received;
        int enq, deq;
        int hashsize;
        int hashused;
    };

    std::vector< PerThread * > threads;
    divine::MpiBase *mpi;
    int pernode, localmin;

    void enqueue( int id ) {
        thread( id ).enq ++;
    }

    void dequeue( int id ) {
        thread( id ).deq ++;
    }

    void hashsize( int id, int s ) {
        thread( id ).hashsize = s;
    }

    void hashadded( int id ) {
        thread( id ).hashused ++;
    }

    PerThread &thread( int id ) {
        assert( id <= threads.size() );
        if ( !threads[ id ] )
            threads[ id ] = new PerThread;
        return *threads[ id ];
    }

    void sent( int from, int to ) {
        assert( to >= 0 );

        PerThread &f = thread( from );
        if ( f.sent.size() <= to )
            f.sent.resize( to + 1, 0 );
            ++ f.sent[ to ];
    }

    void received( int from, int to ) {
        assert( from >= 0 );

        PerThread &t = thread( to );
        if ( t.received.size() <= from )
            t.received.resize( from + 1, 0 );
            ++ t.received[ from ];
    }

    static int first( int a, int ) { return a; }
    static int second( int, int b ) { return b; }
    static int diff( int a, int b ) { return a - b; }

    void matrix( int (*what)(int, int) ) {
        for ( int i = 0; i < threads.size(); ++i ) {
            for ( int j = 0; j < threads.size(); ++j ) {
                std::cerr << " " << std::setw( 9 )
                          << what( thread( i ).sent[ j ], thread( j ).received[ i ] );
            }
            std::cerr << std::endl;
        }
    }

    void label( std::string text, bool d = true ) {
        for ( int i = 0; i < threads.size() - 1; ++ i )
            std::cerr << (d ? "=====" : "-----");
        for ( int i = 0; i < (10 - text.length()) / 2; ++i )
            std::cerr << (d ? "=" : "-");
        std::cerr << " " << text << " ";
        for ( int i = 0; i < (10 - text.length()) / 2; ++i )
            std::cerr << (d ? "=" : "-");
        for ( int i = 0; i < threads.size() - 1; ++ i )
            std::cerr << (d ? "=====" : "-----");
        std::cerr << std::endl;
    }

    void print() {
            label( "QUEUES" );
            matrix( diff );

            label( "local", false );
            for ( int i = 0; i < threads.size(); ++ i )
                std::cerr << " " << std::setw( 9 ) << thread( i ).enq - thread( i ).deq;
            std::cerr << std::endl;

            label( "totals", false );
            for ( int j = 0; j < threads.size(); ++ j ) {
                int t = thread( j ).enq - thread( j ).deq;
                for ( int i = 0; i < threads.size(); ++ i ) {
                    t += thread( i ).sent[ j ] - thread( j ).received[ i ];
                }
                std::cerr << " " << std::setw( 9 ) << t;
            }
            std::cerr << std::endl;

            label( "HASHTABLES" );
            for ( int i = 0; i < threads.size(); ++ i )
                std::cerr << " " << std::setw( 9 ) << thread( i ).hashused;
            std::cerr << std::endl;
            for ( int i = 0; i < threads.size(); ++ i )
                std::cerr << " " << std::setw( 9 ) << thread( i ).hashsize;
            std::cerr << std::endl;
    }

#ifdef HAVE_MPI
    void send() {
        std::vector< int > data;
        data.push_back( localmin );

        for ( int i = 0; i < pernode; ++i ) {
            PerThread &t = thread( i + localmin );
            std::copy( t.sent.begin(), t.sent.end(),
                       std::back_inserter( data ) );
            std::copy( t.received.begin(), t.received.end(),
                       std::back_inserter( data ) );
        }

        mpi->send( &data.front(), data.size(), MPI::INT, 0, TAG_STATISTICS );
        mpi->mpidebug() << "stats sent: " << wibble::str::fmt( data ) << std::endl;
    }

    Loop process( wibble::sys::MutexLock &, MPI::Status &status )
    {
        std::vector< int > data;
        data.resize( pernode * threads.size() * 2 + 1 );

        MPI::COMM_WORLD.Recv( &data.front(), data.size(),
                              MPI::INT, status.Get_source(), status.Get_tag() );
        mpi->mpidebug() << "stats received: " << wibble::str::fmt( data ) << std::endl;

        int min = data.front();

        for ( int i = 0; i < pernode; ++i ) {
            int n = threads.size();
            std::vector< int > sent, received;
            std::vector< int >::iterator first = data.begin() + 1 + i * n * 2;
            std::copy( first, first + n, thread( min + i ).sent.begin() );
            std::copy( first + n, first + 2 * n, thread( min + i ).received.begin() );
        }

        return Continue;
        // TODO
    }
#else
    void send() {}
#endif

    void *main() {
        int i = 0;
        while ( true ) {
            if ( !mpi || mpi->master() ) {
                sleep( 1 );
                print();
            }
            if ( mpi && !mpi->master() ) {
                usleep( 200 * 1000 );
                send();
            }
        }
        return 0;
    }

    Statistics() : mpi( 0 ), pernode( 1 ), localmin( 0 )
    {
        resize( 1 );
    }

    void resize( int s ) {
        std::cerr << "statistics: requested " << s;
        s = std::max( size_t( s ), threads.size() );
        std::cerr << ", using " << s << std::endl;
        threads.resize( s, 0 );
        for ( int i = 0; i < s; ++i ) {
            thread( i ).sent.resize( s, 0 );
            thread( i ).received.resize( s, 0 );
            thread( i ).enq = thread( i ).deq = 0;
            thread( i ).hashsize = thread( i ).hashused = 0;
        }
    }

    template< typename D >
    void useDomain( D &d ) {
        resize( d.n * d.mpi.size() );
        mpi = &d.mpi;
        mpi->registerMonitor( TAG_STATISTICS, *this );
        pernode = d.n;
        localmin = d.minId;
    }

    static Statistics &global() {
        static Statistics *g = new Statistics;
        return *g;
    }
};

}

#endif
