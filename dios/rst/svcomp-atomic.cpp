#include <sys/interrupt.h>

extern "C" void __VERIFIER_atomic_begin() noexcept
{
    __dios_mask( 1 );
}

extern "C" void __VERIFIER_atomic_end() noexcept
{
    __dios_mask( 0 );
}
