// divine-cflags: -std=c++11

#include <atomic>
#include <pthread.h>
#include <cassert>

int x;

int main() {
    pthread_t t;
    pthread_create( &t, nullptr, []( void * ) -> void * {
            ++x;
            return nullptr;
        }, nullptr );
    ++x;
    pthread_join( t, nullptr );
    assert( x == 2 );
}

/* divine-test
holds: false
problem: ASSERTION.*:17
*/
/* divine-test
lart: weakmem:tso:3
holds: false
problem: ASSERTION.*:17
divine: --csdr
*/
/* divine-test
lart: weakmem:std:3
holds: false
problem: ASSERTION.*:17
divine: --csdr
*/
