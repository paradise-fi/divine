// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#pragma once
#include <dios/sys/kernel.hpp>
#include <dios/sys/kobject.hpp>
#include <dios/sys/memory.hpp>

// Syscall argument types
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/monitor.h>
#include <sys/argpad.hpp>
#include <signal.h>

namespace __dios
{

struct SetupBase
{
    MemoryPool *pool;
    const _VM_Env *env;
    SysOpts opts;
};

template< typename Context_ >
struct Setup : SetupBase
{
    using Context = Context_;
    typename Context::Process *proc1;

    Setup( const SetupBase &s ) : SetupBase( s ) {}
};

/* Trap into the kernel and get back out. Each system call gets a separate trap
 * function, and those are part of the configuration. They are instantiated in
 * `dios/config/common.hpp`. DiVM enforces that _VM_CF_Kernel mode is only set
 * by functions annotated with `divine.trap` (available as `__trapfn`, defined
 * in `dios/include/sys/cdefs.h`). */

/* Each of the trap functions is expected to locally instantiate the Trap
 * class, which takes care of the technicalities of the system call protocol
 * (using the RAII idiom). We force the compiler to inline both the ctor and
 * the dtor, because we rely on the exact stack layout and also on the __trapfn
 * annotations which allow us to switch into kernel mode. */

/* Other than that, syscalls are normal function calls (and they are performed
 * using the `call` LLVM instruction). */

struct Trap
{
    enum RM { CONTINUE, RESCHEDULE, TRAMPOLINE } retmode;
    uint64_t keepflags;
    static const uint64_t kern = _VM_CF_KernelMode | _VM_CF_IgnoreLoop | _VM_CF_IgnoreCrit;

    /* Enter the kernel. Nothing too interesting, but if reschedule might
     * happen, we need to sync the _frame pointer of the active task now. */

    __inline Trap( RM rm ) : retmode( rm )
    {
        __sync_synchronize(); // for the sake of weakmem
        keepflags = __vm_ctl_flag( 0, kern ) & kern;
        if ( retmode == CONTINUE )
            return;
        __dios_sync_this_frame();
    }

    /* Leave the kernel. This is a little more complicated in case the syscall
     * needs to return into userspace via a trampoline. The syscall handler
     * creates the trampoline frame on top of the userspace portion of the
     * stack. We must transfer control to the trampoline in a somewhat
     * convoluted way, because it's not executing a call instruction and hence
     * we cannot `ret` into it. */

    __inline ~Trap()
    {
        __vm_ctl_flag( kern & ~keepflags, 0 );
        if ( retmode == RESCHEDULE )
            __dios_reschedule();
        if ( retmode == TRAMPOLINE )
        {
            /* our parent is the __invoke of the sysenter lambda */
            auto frame = __dios_this_frame();
            auto parent = frame->parent;
            auto target = parent->parent;
            /* free the in-between frame */
            __dios_stack_free( parent, target );
            __dios_set_frame( target );
        }
    }
};

/* This is the bottom of the configuration stack. We do 3 things here:
 *  - if a syscall falls through the entire stack and hits the bottom, raise an error
 *  - if any options are left unprocessed during setup(), raise an error
 *  - provide the interface for the Upcall component. */

struct BaseContext : KObject
{
    struct Process : KObject
    {
        virtual ~Process() {}
    };

    template< typename Setup >
    void setup( Setup s )
    {
        traceAlias< BaseContext >( "{BaseContext}" );
        using Ctx = typename Setup::Context;

        if ( s.opts.empty() )
            return;
        for ( const auto& opt : s.opts )
            __dios_trace_f( "ERROR: Unused option %.*s:%.*s",
                            int( opt.first.size() ), opt.first.begin(),
                            int( opt.second.size() ), opt.second.begin() );
        __dios_fault( _DiOS_F_Config, "Unused options" );
    }

    /* The interface to the `Upcall` component. */
    virtual void reschedule() = 0;
    virtual Process *make_process( Process * ) = 0;

    void finalize() {}
    Process *findProcess( pid_t ) { return nullptr; }

    void getHelp( ArrayMap< std::string_view, HelpOption >& ) {}

    /* Instantiate a method for each system call known to us. Implementations
     * in `syscall.cpp`. */

    #include <dios/macro/no_memory_tags>
    #define SYSCALL( name, schedule, ret, arg ) ret name arg;

    #include <sys/syscall.def>

    #undef SYSCALL
    #include <dios/macro/no_memory_tags.cleanup>
};

}
