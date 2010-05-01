// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <wibble/sys/thread.h>
#include <wibble/string.h>
#include <iomanip>

#ifndef DIVINE_STATISTICS_H
#define DIVINE_STATISTICS_H

namespace divine {

struct Statistics : wibble::sys::Thread {
    struct PerThread {
        std::vector< int > sent;
        std::vector< int > received;
        int enq, deq;
    };

    std::vector< PerThread * > threads;

    void enqueue( int id ) {
        thread( id ).enq ++;
    }

    void dequeue( int id ) {
        thread( id ).deq ++;
    }

    PerThread &thread( int id ) {
        assert( id <= threads.size() );
        if ( !threads[ id ] )
            threads[ id ] = new PerThread;
        return *threads[ id ];
    }

    void sent( int from, int to ) {
        PerThread &f = thread( from );
        if ( f.sent.size() <= to )
            f.sent.resize( to + 1, 0 );
            ++ f.sent[ to ];
    }

    void received( int from, int to ) {
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

    void label( std::string text ) {
        for ( int i = 0; i < threads.size() - 1; ++ i )
            std::cerr << "-----";
        for ( int i = 0; i < (10 - text.length()) / 2; ++i )
            std::cerr << "-";
        std::cerr << " " << text << " ";
        for ( int i = 0; i < (10 - text.length()) / 2; ++i )
            std::cerr << "-";
        for ( int i = 0; i < threads.size() - 1; ++ i )
            std::cerr << "-----";
        std::cerr << std::endl;
    }

    void *main() {
        int i = 0;
        while ( true ) {
            sleep( 1 );

            label( "inflight" );
            matrix( diff );
            label( "totals" );
            for ( int j = 0; j < threads.size(); ++ j ) {
                int t = 0;
                for ( int i = 0; i < threads.size(); ++ i ) {
                    t += thread( i ).sent[ j ] - thread( j ).received[ i ];
                }
                std::cerr << " " << std::setw( 9 ) << t;
            }
            std::cerr << std::endl;

            label( "WQ" );
            for ( int i = 0; i < threads.size(); ++ i )
                std::cerr << " " << std::setw( 9 ) << thread( i ).enq - thread( i ).deq;
            std::cerr << std::endl;
        }
    }

    Statistics() {}

    void resize( int s ) {
        threads.resize( s, 0 );
        for ( int i = 0; i < s; ++i ) {
            thread( i ).sent.resize( s );
            thread( i ).received.resize( s );
            thread( i ).enq = thread( i ).deq = 0;
        }
    }

    static Statistics &global() {
        static Statistics *g = new Statistics;
        return *g;
    }
};

}

#endif
