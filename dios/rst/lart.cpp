#include <rst/lart.h>
#include <sys/task.h>
#include <rst/common.h>
#include <sys/divm.h>

#include <string.h>

struct Stash {
    Stash * next;
    uintptr_t val;
};

static inline Stash * __stash_top() {
    return reinterpret_cast< Stash * >( __dios_this_task()->__rst_stash );
}

extern "C"
{
    uintptr_t __lart_unstash()
    {
        auto stash = __stash_top();
        __dios_this_task()->__rst_stash = reinterpret_cast< uintptr_t >( stash->next );
        uintptr_t ret = stash->val;
        __vm_obj_free( stash );
        return ret;
    }

    void __lart_stash( uintptr_t val )
    {
        auto *stash = static_cast< Stash * >( __vm_obj_make( sizeof( Stash ) ) );
        stash->next = __stash_top();
        stash->val = val;
        __dios_this_task()->__rst_stash = reinterpret_cast< uintptr_t >( stash );
    }
}
