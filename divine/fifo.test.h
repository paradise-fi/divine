// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/fifo.h>

using namespace divine;

struct TestFifo {
    struct Checker : wibble::sys::Thread
    {
        divine::Fifo< int > fifo;
        int terminate;
        int n;
        
        void *main()
        {
            int x[ n ];
            int j = 0;
            for ( int i = 0; i < n; ++i )
                x[ i ] = 0;

            while (true) {
                while ( !fifo.empty() ) {
                    int i = fifo.front();
                    assert_eq( x[i % n], i / n );
                    ++ x[ i % n ];
                    fifo.pop();
                }
                sched_yield();
                if ( terminate > 1 )
                    break;
                if ( terminate )
                    ++terminate;
            }
            terminate = 0;
            for ( int i = 0; i < n; ++i )
                assert_eq( x[ i ], 1024*1024 );
            return 0;
        }

        Checker( int _n = 1 ) : terminate( 0 ), n( _n ) {}
    };

    struct Pusher : wibble::sys::Thread
    {
        divine::Fifo< int > &fifo;
        int id;
        int n;

        void *main()
        {
            for( int i = 0; i < 1024 * 1024; ++i )
                fifo.push( i * n + id );
            return 0;
        }

        Pusher( Fifo< int > &f, int _id, int _n ) : fifo( f ), id( _id ), n( _n ) {}
    };

    Test stress() {
        Checker c;
        int j = 0;
        for ( int j = 0; j < 5; ++j ) {
            c.start();
            for( int i = 0; i < 1024 * 1024; ++i )
                c.fifo.push( i );
            c.terminate = true;
            c.join();
        }
    }

    Test multiStress() {
        int n = 10;
        Checker c( n );
        std::vector< Pusher* > p;
        for ( int i = 0; i < n; ++i )
            p.push_back( new Pusher( c.fifo, i, n ) );
        c.start();
        for ( int i = 0; i < n; ++i )
            p[ i ]->start();
        for ( int i = 0; i < n; ++i )
            p[ i ]->join();
        c.terminate = true;
        c.join();
    }
};
