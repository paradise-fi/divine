// VERIFY_OPTS: --capture `dirname $1`/medium:follow:/t

#include <dios.h>
#include <cstring>
#include <cassert>
#include <regex>
#include <dios/stdlibwrap.hpp>

#include "vfs-capture-assert.h"

extern "C" void __boot( const _VM_Env *env ) {
    dset< dstring > expected({ "/t", "/t/symlinkB", "/t/a", "/t/b", "/t/c",
        "/t/printing.cpp" });

    auto context = __dios::new_object< Context >();
    __vm_trace( _VM_T_StateType, context );
    __vm_control( _VM_CA_Set, _VM_CR_State, context );
    __vm_control( _VM_CA_Set, _VM_CR_Scheduler, passSched );

    auto names = collectVfsNames( env );
    bAss( names.second, context, "Multi-capture of an object" );
    for ( const auto& item : expected )
        __dios_trace_f( "E: %s", item.c_str() );

    bAss( names.first == expected, context, "Different files captures" );
}
