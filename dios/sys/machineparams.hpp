// -*- C++ -*- (c) 2017 Jan Mrázek <email@honzamrazek.cz>

#pragma once

#include <dios/sys/options.hpp>
#include <sys/utsname.h>

namespace __dios {

template < typename Next >
struct MachineParams : Next
{
    int hardwareConcurrency;

    template< typename Setup >
    void setup( Setup s )
    {
        traceAlias< MachineParams >( "{MachineParams}" );
        hardwareConcurrency = 0;
        readHardwareConcurrency( s.opts );
        if ( extractOpt( "debug", "machineparams", s.opts ) )
        {
            traceConfig( 1 );
            __vm_ctl_flag( 0, _VM_CF_Error );
        }

        Next::setup( s );
    }

    void getHelp( ArrayMap< std::string_view, HelpOption >& options )
    {
        const char *opt = "ncpus";
        if ( options.find( opt ) != options.end() ) {
            __dios_trace_f( "Option %s already present", opt );
            __dios_fault( _DiOS_F_Config, "Option conflict" );
        };

        options[ { opt } ] = { "specify number of cpu units (default 0)",
            { "<num>" } };
        Next::getHelp( options );
    }

    void readHardwareConcurrency( SysOpts& opts )
    {
        std::string_view ncpus = extractOpt( "ncpus", opts );
        if ( !ncpus.empty() )
        {
            char *end;
            hardwareConcurrency = _DIVINE_strtol( ncpus.begin(), ncpus.size(), &end );
            if ( ncpus.begin() == end || end - 1 != &ncpus.back() )
                __dios_fault( _DiOS_F_Config,
                    "DiOS boot configuration: invalid ncpus specified" );
        }
    }

    void traceConfig( int indent )
    {
        __dios_trace_i( indent, "machine parameters:" );
        __dios_trace_i( indent + 1, "ncpus: %d", hardwareConcurrency );
    }

    int hardware_concurrency()
    {
        return hardwareConcurrency;
    }

    int uname( struct ::utsname * name )
    {
        strcpy( name->sysname, "DiOS" );
        strcpy( name->nodename, "" );
        strcpy( name->release, "0.1" );
        strcpy( name->version, "0" );
        strcpy( name->machine, "DIVINE 4 VM" );
        return 0;
    }
};

} // namespace __dios
