#include <sys/divm.h>

extern "C" void __VERIFIER_assume( int expression ) noexcept
{
    if ( !expression )
        __vm_cancel();
}
