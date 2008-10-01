#include <wibble/sys/thread.h>
#include <divine/threading.h>
#include <iostream>

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

int main( int argc, char **argv )
{
    Checker c;
    int j = 0;
    while ( true ) {
        c.start();
        for( int i = 0; i <= 1024 * 1024; ++i )
            c.fifo.push( i );
        c.terminate = true;
        c.join();
        std::cout << "iteration " << j << "\r" << std::flush;
        ++ j;
    }
}
