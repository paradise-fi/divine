#include "demo.h"
#include <thread>

int main() {
    int x = 0;
    puts( "starting thread" );
    std::thread t1( [&] {
        puts( "thread started" );
        ++x;
        puts( "thread done" );
    } );
    puts( "incrementing" );
    ++x;
    puts( "waiting for the thread" );
    t1.join();
    puts( "thread joined" );
    assert( x == 2 ); /* ERROR */
}
