/* TAGS: min c++ */
// VERIFY_OPTS: -o nofail:malloc

#include <thread>
#include <atomic>
#include <cassert>
#include <sys/divm.h>

std::atomic_int a;

int main() {
    std::thread t( [] {
            a = 1;
        } );
    int r1 = a;
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask );
    int r2 = a;
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_None );
    assert( r1 == r2 ); /* ERROR */
    t.join();
}
