#include <wibble/sys/thread.h>
#include <stdlib.h>
#include <cassert>
#include <iostream>

struct Thrasher : wibble::sys::Thread
{
    long long padding1;
    long long terminate, total;
    unsigned seedp, seedp_pad;
    long long iter;
    long long padding2;
    void *main()
    {
        int bound = 1024*1024*(2500/total);
        char *work = new char[bound+1];
        int r;
        while (!terminate) {
            r = rand_r(&seedp);
            if ( (r/bound) % 2 )
                work[r%bound] = r;
            else
                r = work[r%bound];
            work[(r+iter)%bound] = iter;
            ++ iter;
        }
        return 0;
    }

    Thrasher( int t, unsigned s ) : terminate( 0 ), iter( 0 ),
                                    total( t ), seedp( s ) {}
};

int main( int argc, char **argv )
{
    assert( argc == 2 );
    int t = atoi( argv[ 1 ] );
    std::vector< Thrasher > v;
    for ( int i = 0; i < t; ++i )
        v.push_back( Thrasher( t, i ) );
    for ( int i = 0; i < t; ++i )
        v[i].start();
    sleep( 10 );
    for ( int i = 0; i < t; ++i )
        v[i].terminate = 1;
    for ( int i = 0; i < t; ++i )
        v[i].join();
    long long sum = 0;
    for ( int i = 0; i < t; ++i )
        sum += (v[i].iter/1024);
    std::cout << "total iterations: " << sum << "k" << std::endl;
}
