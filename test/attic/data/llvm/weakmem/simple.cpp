// divine-cflags: -std=c++11

#include <atomic>
#include <cassert>

int x;

int main() {
    __divine_new_thread( []( void * ) { ++x; }, nullptr );
    return x;
}

/* divine-test
holds: false
problem: PROBLEM.*_PDCLIB_Exit
*/
/* divine-test
lart: weakmem:tso:3
holds: false
problem: PROBLEM.*_PDCLIB_Exit
*/
/* divine-test
lart: weakmem:std:3
holds: false
problem: PROBLEM.*_PDCLIB_Exit
*/
