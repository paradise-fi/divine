#include <wibble/sys/thread.h>
#include <divine/pool.h>
#include <deque>
#include <iostream>

struct Checker : wibble::sys::Thread
{
    char padding[128];
    divine::Pool pool;
    std::deque< char * > ptrs;
    int limit;
    unsigned seedp;
    int terminate;
    char padding2[128];

    bool decide( int i ) {
        int j = rand() % limit;
        if ( i + j > limit )
            return false;
        return true;
    }

    void *main()
    {
        limit = 1024*1024;
        for ( int i = 0; i < limit; ++i ) {
            if ( decide( i ) || ptrs.empty() ) {
                ptrs.push_back( pool.alloc( 32 ) );
            } else {
                pool.free( ptrs.front(), 32 );
                ptrs.pop_front();
            }
        }
        return 0;
    }

    Checker() : terminate( 0 ) {}
};

int main( int argc, char **argv )
{
    std::vector< Checker > c;
    assert( argc == 2 );
    c.resize( atoi( argv[ 1 ] ) );
    int j = 0;
    while ( true ) {
        for ( int i = 0; i < c.size(); ++i )
            c[ i ].start();
        for ( int i = 0; i < c.size(); ++i )
            c[ i ].join();
        std::cout << "iteration " << j << "\r" << std::flush;
        ++ j;
    }
}
