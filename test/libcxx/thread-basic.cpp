/* TAGS: min threads c++ */
#include <thread>
#include <cassert>
#include <stdexcept>

#ifdef __divine__
#include <dios.h>

__attribute__((constructor)) static void disbaleMallocFail() {
    __dios_configure_fault( _DiOS_SF_Malloc, _DiOS_FC_NoFail );
}
#endif

volatile int x = 0;

int main() {
    std::thread t( [] { x = 1; } );
    t.join();
    assert( x == 1 );
}
