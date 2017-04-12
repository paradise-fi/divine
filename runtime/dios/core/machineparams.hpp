// -*- C++ -*- (c) 2017 Jan Mrázek <email@honzamrazek.cz>

#pragma once

#include <dios/core/memory.hpp>
#include <dios/kernel.hpp>
#include <dios/core/main.hpp>

namespace __dios {

template < typename Next >
struct MachineParams: public Next {
    int hardwareConcurrency;

    void linkSyscall( BaseContext::SyscallInvoker invoker ) {
        Next::linkSyscall( invoker );
    }

    void setup( MemoryPool& pool, const _VM_Env *env, SysOpts opts ) {
        hardwareConcurrency = 0;
        readHardwareConcurrency( opts );
        if ( extractOpt( "debug", "machineparams", opts ) ) {
            traceConfig( 1 );
            __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Error, _VM_CF_Error );
        }

        Next::setup( pool, env, opts );
    }

    void finalize() {
        Next::finalize();
    }

    void readHardwareConcurrency( SysOpts& opts )  {
        String ncpus = extractOpt( "ncpus", opts );
        if ( !ncpus.empty() ) {
            char *end;
            hardwareConcurrency = strtol( ncpus.c_str(), &end, 10 );
            if ( ncpus.data() == end || end - 1 != &ncpus.back() )
                __dios_fault( _DiOS_F_Config,
                    "DiOS boot configuration: invalid ncpus specified" );
        }
    }

    void traceConfig( int indent ) {
        __dios_trace_i( indent, "machine parameters:" );
        __dios_trace_i( indent + 1, "ncpus: %d", hardwareConcurrency );
    }

    int hardware_concurrency() {
        return hardwareConcurrency;
    }

    int uname( struct ::utsname * name ) {
        strcpy( name->sysname, "DiOS" );
        strcpy( name->nodename, "" );
        strcpy( name->release, "0.1" );
        strcpy( name->version, "0" );
        strcpy( name->machine, "DIVINE 4 VM" );
        return 0;
    }
};

} // namespace __dios
