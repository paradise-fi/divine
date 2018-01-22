/* TAGS: min threads c++ */
#include <thread>
#include <cassert>

#ifdef __divine__
#include <dios.h>

__attribute__((constructor)) static void disbaleMallocFail() {
    __dios_configure_fault( _DiOS_SF_Malloc, _DiOS_FC_NoFail );
}
#endif

volatile int x;

int main() {
    std::thread t1( [] { ++x; } );
    std::thread t2( [] { ++x; } );
    assert( x == 0 || x == 1 || x == 2 );
    assert( x == 2 ); /* ERROR */
    t2.join();
    t1.join();
}

