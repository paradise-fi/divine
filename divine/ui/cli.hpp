// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
 * (c) 2016 Viktória Vozárová <>
 * (c) 2016 Vladimír Štill <xstill@fi.muni.cz>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include <divine/mc/bitcode.hpp>
#include <divine/mc/job.hpp>
#include <divine/cc/options.hpp>
#include <divine/rt/dios-cc.hpp>
#include <divine/sim/trace.hpp>

#include <divine/ui/common.hpp>
#include <divine/ui/curses.hpp>
#include <divine/ui/die.hpp>
#include <divine/ui/log.hpp>

#include <brick-cmd>
#include <brick-fs>
#include <brick-string>
#include <cctype>
#include <regex>

#include <divine/vm/divm.h>

namespace divine::ui::arg
{
    struct mem
    {
        size_t size;
        mem( size_t s = 0 ) : size( s ) {}
    };

    struct mount
    {
        std::string capture;
        std::string mount;
        bool followSymlink;
    };

    template< typename type >
    struct commasep
    {
        std::vector< type > vec;
    };

    enum class report { none, yaml, yaml_long };

    static brq::parse_result from_string( std::string_view s, mem &m )
    {
        size_t pos;
        m.size = stoull( std::string( s ), &pos );
        auto r = s.substr( pos );

        if ( r.empty() )
            return {};

        std::string rem;
        std::transform( r.begin(), r.end(), std::back_inserter( rem ), ::tolower );
        if ( rem == "k" || rem == "kb" )
            m.size *= 1000;
        else if ( rem == "ki" || rem == "kib" )
            m.size *= 1024;
        else if ( rem == "m" || rem == "mb" )
            m.size *= 1000'000;
        else if ( rem == "mi" || rem == "mib" )
            m.size *= 1024 * 1024;
        else if ( rem == "g" || rem == "gb" )
            m.size *= 1'000'000'000;
        else if ( rem == "gi" || rem == "gib" )
            m.size *= 1024 * 1024 * 1024;
        else
            return brq::no_parse( "unknown memory size suffix", r );

        return {};
    }

    static brq::parse_result from_string( std::string_view s, mount &m )
    {
        m.followSymlink = true;
        std::regex sep( ":" );
        std::regex_token_iterator it( s.begin(), s.end(), sep, -1 );
        int i;

        for ( i = 0; it != decltype( it )(); it++, i++ )
            switch ( i )
            {
                case 0:
                    m.capture = *it;
                    m.mount = *it;
                    break;
                case 1:
                    if ( *it == "follow" )
                        m.followSymlink = true;
                    else if ( *it == "nofollow" )
                        m.followSymlink = false;
                    else
                        return brq::no_parse( "invalid option for follow links in", s );
                    break;
                case 2:
                    m.mount = *it;
                    break;
                default:
                    return brq::no_parse( "unexpected attribute", *it, "in", s );
            }

        if ( i < 1 )
            return brq::no_parse( "the directory to capture is missing in", s );
        if ( !brq::file_access( m.capture, F_OK ) )
            return brq::no_parse( "file or directory", m.capture, "does not exist" );
        if ( !brq::file_access( m.capture, R_OK ) )
            return brq::no_parse( "file or directory", m.capture, "is not readable" );

        return {};
    }

    template< typename T >
    brq::parse_result from_string( std::string_view s, commasep< T > &cs )
    {
        using brq::from_string;
        T val;

        for ( auto c : brq::splitter( s, ',' ) )
        {
            if ( auto r = from_string( c, val ) ; !r )
                return r;
            else
                cs.vec.push_back( val );
        }

        return {};
    }

    static brq::parse_result from_string( std::string_view s, report &r )
    {
        if      ( s == "none" ) r = report::none;
        else if ( s == "yaml" ) r = report::yaml;
        else if ( s == "yaml-long" ) r = report::yaml_long;
        else return brq::no_parse( "report type must be none, yaml or yaml-long" );
        return {};
    }
}

namespace divine::ui
{
    struct command : brq::cmd_base
    {
        virtual void setup() {}
        virtual void cleanup() {}
        virtual ~command() {}
    };

    struct with_bc : command
    {
        mc::BCOptions _bc_opts;

        std::string _std;
        brq::cmd_file _stdin;

        // Used to build the final compiler options in _bc_opts.ccopts
        std::vector< std::string > _env;
        std::vector< std::string > _useropts;
        std::vector< std::string > _systemopts;
        std::vector< std::string > _linkLibs;
        std::vector< arg::commasep< std::string > > _ccOpts;
        std::vector< arg::mount > _vfs;
        arg::mem _vfs_limit = 16 * 1024 * 1024;
        bool _init_done = false;
        SinkPtr _log = nullsink();
        std::string _dump_bc;
        rt::DiosCC _cc_driver;

        virtual void process_options();
        void report_options();
        void setup();
        void init();

        std::shared_ptr< mc::BitCode > bitcode()
        {
            if ( !_init_done )
                init();
            _init_done = true;
            return _bc;
        }

        void options( brq::cmd_options &c ) override
        {
            command::options( c );
            c.section( "Compiler Options" );
            c.opt( "-C,", _ccOpts ) << "pass additional options to the compiler";
            c.opt( "-std=", _std ) << "set the C/C++ standard to use";
            c.opt( "-l", _linkLibs ) << "link additional libraries, e.g. -lm for libm";

            c.section( "Execution Environment" );
            c.opt( "-D", _env ) << "set an environment variable";
            c.opt( "-o", _systemopts ) << "pass options to dios";
            c.opt( "--vfs-limit", _vfs_limit ) << "maximal filesystem snapshot size [16MiB]";
            c.opt( "--capture", _vfs ) << "capture parts of the filesystem";
            c.opt( "--stdin", _stdin ) << "capture a file for use as standard input";
            c.opt( "--dios-config", _bc_opts.dios_config ) << "select a dios config manually";

            c.section( "Bitcode Transforms" );
            c.flag( "--static-reduction", _bc_opts.static_reduction )
                 << "transform for smaller state space [default: yes]";
            c.opt( "--autotrace",      _bc_opts.autotrace ) << "trace function calls";
            c.opt( "--leakcheck",      _bc_opts.leakcheck ) << "insert memory leak checks";
            c.opt( "--sequential",     _bc_opts.sequential ) << "disable support for threading";
            c.opt( "--synchronous",    _bc_opts.synchronous ) << "enable synchronous mode";
            c.opt( "--relaxed-memory", _bc_opts.relaxed )
                << "allow memory operation reordering (x86[:depth] or tso[:depth])";
            c.opt( "--lart", _bc_opts.lart_passes ) << "run additional LART passes";
            c.opt( "--symbolic", _bc_opts.symbolic ) << "enable semi-symbolic data representation";
            c.opt( "--svcomp", _bc_opts.svcomp ) << "work around SV-COMP quirks";
            c.opt( "--dump-bc", _dump_bc ) << "dump the transformed bitcode into a file";
            c.pos( _bc_opts.input_file );
            c.collect( _useropts );
        }

    private:
        std::shared_ptr< mc::BitCode > _bc;
    };

    struct version : command
    {
        void run() override;
    };

    struct ltlc : command
    {
        std::string _automaton, _formula, _output, _system;
        brq::cmd_flag _negate;
        void run() override;

        void options( brq::cmd_options &c ) override
        {
            command::options( c );
            c.opt( "--formula", _formula );
            c.opt( "--negate", _negate );
            c.opt( "--output", _output );
            c.opt( "--system", _system );
        }
    };

    struct verify : with_bc
    {
        arg::mem _max_mem = 0; // bytes
        int _max_time = 0;  // seconds
        int _threads = 0;
        int _num_callers = 10;
        int _poolstat_period = 0;
        brq::cmd_flag _counterexample = true, _liveness, _do_report_file = true, _report_unique;
        bool _interactive = true;
        arg::report _report = arg::report::yaml;
        std::string _solver = "stp";

        std::shared_ptr< std::ostream > _report_file;
        brq::cmd_path _report_filename;
        void setup_report_file();

        void setup() override;
        void run() override;
        void cleanup() override;

        void safety();
        void liveness();
        void print_ce( mc::Job &job );

        void options( brq::cmd_options &c ) override
        {
            with_bc::options( c );
            c.section( "Verification Options" );
            c.opt( "--threads", _threads ) << "number of worker threads to use";
            c.opt( "--max-memory", _max_mem ) << "set a memory limit";
            c.opt( "--max-time", _max_time ) << "set a time limit (in seconds)";
            c.opt( "--liveness", _liveness ) << "enable verification of liveness properties";
            c.opt( "--solver", _solver ) << "select a constraint solver to use in --symbolic mode";

            c.section( "Reporting" );
            c.opt( "--report", _report ) << "type of report to print";
            c.flag( "--report-file", _do_report_file ) << "write the report into a file [yes]";
            c.flag( "--counterexample", _counterexample ) << "generate a counterexample [yes]";
            c.opt( "--report-unique", _report_unique ) << "generate a unique name for the report [no]";
            c.opt( "--report-filename", _report_filename ) << "use the specified file name";
            c.opt( "--num-callers", _num_callers ) << "number of frames to print in backtraces [10]";
        }
    };

    struct check : verify
    {
        void setup() override;
    };

    struct exec : with_bc
    {
        brq::cmd_flag _trace, _virtual;

        void setup();
        void run();

        void options( brq::cmd_options &c ) override
        {
            with_bc::options( c );
            c.section( "Exec Options" );
            c.opt( "--virtual", _virtual ) << "simulate system calls instead of executing them";
            c.opt( "--trace", _trace ) << "print instructions as they are executed";
        }
    };

    struct sim : with_bc
    {
        brq::cmd_flag _batch, _skip_init, _load_report;
        std::shared_ptr< divine::sim::Trace > _trace;

        void options( brq::cmd_options &c ) override
        {
            with_bc::options( c );
            c.section( "Sim Options" );
            c.opt( "--batch", _batch ) << "disable interactive features";
            c.opt( "--load-report", _load_report ) << "load a counterexample from a report";
            c.opt( "--skip-init", _skip_init ) << "do not load ~/.divine/sim.init";
        }

#if OPT_SIM
        void process_options() override;
        void run() override;
#else
        void process_options() {}
        void run()
        {
            throw std::runtime_error( "This build of DIVINE does not support the 'sim' command." );
        }
#endif
    };

    struct draw : with_bc
    {
        int _number = 0;
        int _distance = 32;
        enum { All, None, Trace } _labels = None;
        bool _bfs = false, _raw = false;
        std::string _render = "dot -Tx11";
        std::string _output = "-";

        void run() override;

        void options( brq::cmd_options &c ) override
        {
            with_bc::options( c );
            c.section( "Draw Options" );
            c.opt( "--distance", _distance ) << "maximum distance from the initial state [32]";
            c.opt( "--render", _render ) << "command to pipe the 'dot' output into [dot -Tx11]";
        }
    };

    struct cc : command
    {
        cc() = default;
        cc( cc && ) = default;
        cc& operator=( cc&& ) = default;

        divine::cc::Options _opts;
        std::vector< std::string > _flags;
        std::vector< arg::commasep< std::string > > _passThroughFlags;
        std::string _output;
        rt::DiosCC _driver;

        void run();

        void options( brq::cmd_options &c ) override
        {
            command::options( c );
            c.opt( "-c", _opts.dont_link ) << "compile but do not link";
            c.opt( "--dont-link", _opts.dont_link ) << "alias for the above";
            c.opt( "-o", _output ) << "write the output into a given file";
            c.opt( "-C,", _passThroughFlags ) << "pass additional options to the compiler";
            c.collect( _flags );
        }
    };

    struct info : exec
    {
        info()
        {
            _systemopts.push_back( "help" );
        }

        void run() override;
    };

    static std::string outputName( std::string path, std::string ext )
    {
        return brq::replace_extension( brq::basename( path ), ext );
    }

    std::shared_ptr< Interface > make_cli( int argc, const char **argv );
}
