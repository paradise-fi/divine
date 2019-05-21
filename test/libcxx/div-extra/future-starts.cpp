/* TAGS: min c++ threads */
#include <future>
#include <cassert>

#ifdef __divine__
#include <dios.h>

__attribute__((constructor)) static void disbaleMallocFail() {
    __dios_configure_fault( _DiOS_SF_Malloc, _DiOS_FC_NoFail );
}
#endif

int main() {
    std::future< int > f = std::async( std::launch::async, [](){
            assert( false ); /* ERROR */
            return 8;
        } );
    assert( f.get() == 8 );
}
