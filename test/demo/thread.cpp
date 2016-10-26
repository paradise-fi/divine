#include "demo.h"
#include <thread>

int main() {
    int x = 0;
    print( "starting thread" );
    std::thread t1( [&] {
        print( "thread started" );
        ++x;
        print( "thread done" );
    } );
    print( "incrementing" );
    ++x;
    print( "waiting for the thread" );
    t1.join();
    print( "thread joined" );
    assert( x == 2 ); /* ERROR */
}
