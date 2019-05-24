/* TAGS: min c++ todo */
// VERIFY_OPTS: --capture `dirname $1`/medium:follow:/t -o nofail:malloc

/* TODO: the symlink is not actually in the repository/tarballs/... and we need
 * to figure a way to create it for testing purposes */

#include <dios.h>
#include <cstring>
#include <cassert>
#include <regex>
#include <dios/sys/stdlibwrap.hpp>
#include <sys/trace.h>

#include "vfs-capture-assert.h"

extern "C" void __dios_boot( const _VM_Env *env )
{
    std::set< std::string > expected({ "/t", "/t/symlinkB", "/t/a", "/t/b", "/t/c", "/t/printing.cpp" });

    auto context = new Context();
    __vm_trace( _VM_T_StateType, context );
    __vm_ctl_set( _VM_CR_State, reinterpret_cast< void * >( context ) );
    __vm_ctl_set( _VM_CR_Scheduler, reinterpret_cast< void * >( passSched ) );

    auto names = collectVfsNames( env );
    bAss( names.second, context, "Multi-capture of an object" );
    for ( const auto& item : expected )
        __dios_trace_f( "E: %s", item.c_str() );

    bAss( names.first == expected, context, "Different files captures" );
    __vm_suspend();
}
