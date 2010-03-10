// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/fifo.h>
#include <wibble/sys/thread.h>

using namespace divine;

struct TestFifo {
    template< typename T >
    struct Checker : wibble::sys::Thread
    {
        divine::Fifo< T > fifo;
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
                    int i = unblob< int >( fifo.front() );
                    assert_eq( x[i % n], i / n );
                    ++ x[ i % n ];
                    fifo.pop();
                }
#ifdef POSIX
                sched_yield();
#endif
                if ( terminate > 1 )
                    break;
                if ( terminate )
                    ++terminate;
            }
            terminate = 0;
            for ( int i = 0; i < n; ++i )
                assert_eq( x[ i ], 128*1024 );
            return 0;
        }

        Checker( int _n = 1 ) : terminate( 0 ), n( _n ) {}
    };

    Test stress() {
        Checker< int > c;
        int j = 0;
        for ( int j = 0; j < 5; ++j ) {
            c.start();
            for( int i = 0; i < 128 * 1024; ++i )
                c.fifo.push( i );
            c.terminate = true;
            c.join();
        }
    }

    Test blobStress() {
        Checker< Blob > c;
        int j = 0;
        for ( int j = 0; j < 5; ++j ) {
            c.start();
            for( int i = 0; i < 128 * 1024; ++i ) {
                Blob b( sizeof( int ) );
                b.get< int >() = i;
                c.fifo.push( b );
            }
            c.terminate = true;
            c.join();
        }
    }
};
