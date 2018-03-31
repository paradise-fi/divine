#include <sys/fault.h>

extern "C" void __VERIFIER_error() noexcept
{
    __dios_fault( _DiOS_F_Assert, "verifier error called" );
}
