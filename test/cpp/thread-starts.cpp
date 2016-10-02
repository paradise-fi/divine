#include <thread>
#include <cassert>

#ifdef __divine__
#include <dios.h>

__attribute__((constructor)) static void disbaleMallocFail() {
    __dios_configure_fault( _DiOS_SF_Malloc, _DiOS_FC_NoFail );
}
#endif

int main() {
    std::thread t( [] {
                assert( false ); /* ERROR */
            } );
    t.join();
}
