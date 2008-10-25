// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/fifo.h>

using namespace divine;

struct TestFifo {
    struct Checker : wibble::sys::Thread
    {
        divine::Fifo< int > fifo;
        int terminate;
        
        void *main()
        {
            int j = 0, i = 0;
            while (true) {
                while (! fifo.empty() ) {
                    i = fifo.front();
                    assert( i == j );
                    ++ j;
                    fifo.pop();
                }
                sched_yield();
                if ( terminate > 1 )
                    break;
                if ( terminate )
                    ++terminate;
            }
            terminate = 0;
            assert( i == 1024*1024 );
            return 0;
        }

        Checker() : terminate( 0 ) {}
    };

    Test stress() {
        Checker c;
        int j = 0;
        for ( int j = 0; j < 10; ++j ) {
            c.start();
            for( int i = 0; i <= 1024 * 1024; ++i )
                c.fifo.push( i );
            c.terminate = true;
            c.join();
        }
    }
};
