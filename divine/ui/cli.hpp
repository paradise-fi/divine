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

#include <divine/vm/bitcode.hpp>
#include <divine/cc/options.hpp>

#include <divine/ui/common.hpp>
#include <divine/ui/curses.hpp>
#include <divine/ui/die.hpp>
#include <divine/ui/version.hpp>

#include <brick-cmd>
#include <brick-fs>
#include <brick-string>
#include <cctype>
#include <regex>

#include <runtime/divine.h>

namespace divine {
namespace ui {

using namespace std::literals;
namespace cmd = brick::cmd;

struct Command
{
    virtual void run() = 0;
    virtual void setup() {}
    virtual ~Command() {}
};

struct WithBC : Command
{
    struct VfsDir {
        std::string capture;
        std::string mount;
        bool followSymlink;
    };

    std::string _file, _std, _stdin;
    std::vector< std::string > _env;
    std::vector< std::string > _useropts;
    std::vector< std::string > _systemopts;
    std::vector< std::string > _lartPasses;
    std::vector< std::vector< std::string > > _ccOpts;
    std::vector< VfsDir > _vfs;
    size_t _vfsSizeLimit;
    vm::AutoTraceFlags _autotrace;
    bool _disableStaticReduction = false;
    bool _init_done = false;

    void setup();
    std::shared_ptr< vm::BitCode > bitcode()
    {
        if ( !_init_done )
            _bc->init( true );
        _init_done = true;
        return _bc;
    }

    WithBC() : _vfsSizeLimit( 16 * 1024 * 1024 ) {}

private:
    std::shared_ptr< vm::BitCode > _bc;
};

struct Help
{
    std::string _cmd = std::string("");

    template< typename P >
    void run( P cmds )
    {
        std::string description = cmds.describe( _cmd );
        if ( description.empty() && !_cmd.empty() )
            die( "Unknown command '" + _cmd + "'. Available commands are:\n" + cmds.describe() );
        if ( _cmd.empty() )
        {
            std::cerr << "To print details about a specific command, run 'divine help {command}'.\n\n";
            std::cerr << cmds.describe() << std::endl;
        }
        else std::cerr << description << std::endl;
    }
};

struct Version : Command {

    void run() override;
};

struct Verify : WithBC
{
    int _max_mem = 0; // MB
    int _max_time = 0;  // seconds
    int _threads = 0;
    int _backtraceMaxDepth = 10;
    bool _no_counterexample = false;
    bool _report = false;

    void run();
};

struct Run : WithBC {
    void run();
};

struct Sim : WithBC
{
    bool _batch = false;

    Sim() { _systemopts.emplace_back( "trace:thread" ); }
    void run();
};

struct Draw : WithBC
{
    int _number = 0;
    int _distance = 32;
    enum { All, None, Trace } _labels = None;
    bool _bfs = false, _raw = false;
    std::string _render = "dot -Tx11"s;
    std::string _output = "-"s;

    void run();
};

struct Cc : Command
{
    cc::Options _drv;
    std::vector< std::string > _flags;
    std::vector< std::vector< std::string > > _passThroughFlags;
    std::string _output;

    void run();
};

struct DivineCc : Command
{
    void run() override;

    std::string _output;
    bool _dontLink = false, _splitCompilation = false;
    std::vector< std::string > _flags;
    std::vector< std::string > _libPaths;
};

struct DivineLd : Command
{
    void run() override;

    std::string _output;
    bool _incremental = false, _merge = false;
    std::vector< std::string > _flags;
};

struct Info   : Run
{
    Info() {
        _systemopts.push_back( "help" );
    }

    void run() override;
};

namespace {

std::vector< std::string > fromArgv( int argc, const char **argv )
{
    std::vector< std::string > args;
    std::copy( argv + 1, argv + argc, std::back_inserter( args ) );
    return args;
}

size_t memFromString( std::string s ) {
    size_t pos;
    size_t base = stoull( s, &pos );
    std::string r = s.substr( pos );
    if ( r.empty() )
        return base;
    std::string rem;
    std::transform( r.begin(), r.end(), std::back_inserter( rem ), ::tolower );
    if ( rem == "k" || rem == "kb" )
        base *= 1000;
    else if ( rem == "ki" || rem == "kib" )
        base *= 1024;
    else if ( rem == "m" || rem == "mb" )
        base *= 1000'000;
    else if ( rem == "mi" || rem == "mib" )
        base *= 1024 * 1024;
    else if ( rem == "g" || rem == "gb" )
        base *= 1'000'000'000;
    else if ( rem == "gi" || rem == "gib" )
        base *= 1024 * 1024 * 1024;
    else
        throw std::invalid_argument( "unknown suffix " + r );
    return base;
}

}

struct CLI : Interface
{
    bool _batch;
    std::vector< std::string > _args;

    CLI( int argc, const char **argv ) :
        _batch( true ), _args( fromArgv( argc, argv ) )
    { }

    CLI( std::vector< std::string > args ) :
        _batch( true ), _args( args )
    { }

    auto validator()
    {
        return cmd::make_validator()->
            add( "file", []( std::string s, auto good, auto bad )
                 {
                     if ( s[0] == '-' ) /* FIXME! zisit, kde sa to rozbije */
                         return bad( cmd::BadFormat, "file must not start with -" );
                     if ( !brick::fs::access( s, F_OK ) )
                         return bad( cmd::BadContent, "file " + s + " does not exist");
                     if ( !brick::fs::access( s, R_OK ) )
                         return bad( cmd::BadContent, "file " + s + " is not readable");
                     return good( s );
                 } ) ->
            add( "vfsdir", []( std::string s, auto good, auto bad )
                {
                    WithBC::VfsDir dir { .followSymlink = true };
                    std::regex sep(":");
                    std::sregex_token_iterator it( s.begin(), s.end(), sep, -1 );
                    int i;
                    for ( i = 0; it != std::sregex_token_iterator(); it++, i++ )
                        switch ( i ) {
                        case 0:
                            dir.capture = *it;
                            dir.mount = *it;
                            break;
                        case 1:
                            if ( *it == "follow" )
                                dir.followSymlink = true;
                            else if ( *it == "nofollow" )
                                dir.followSymlink = false;
                            else
                                return bad( cmd::BadContent, "invalid option for follow links" );
                            break;
                        case 2:
                            dir.mount = *it;
                            break;
                        default:
                            return bad( cmd::BadContent, " unexpected attribute "
                                + std::string( *it ) + " in vfsdir" );
                    }

                    if ( i < 1 )
                        return bad( cmd::BadContent, "missing a directory to capture" );
                    if ( !brick::fs::access( dir.capture, F_OK ) )
                        return bad( cmd::BadContent, "file or directory " + dir.capture + " does not exist" );
                    if ( !brick::fs::access( dir.capture, R_OK ) )
                        return bad( cmd::BadContent, "file or directory " + dir.capture + " is not readable" );
                    return good( dir );
                } ) ->
            add( "mem", []( std::string s, auto good, auto bad )
                {
                    try {
                        size_t size = memFromString( s );
                        return good( size );
                    }
                    catch ( const std::invalid_argument& e ) {
                        return bad( cmd::BadContent, std::string("cannot read size: ")
                            + e.what() );
                    }
                    catch ( const std::out_of_range& e ) {
                        return bad( cmd::BadContent, "size overflow" );
                    }
                } ) ->
            add( "label", []( std::string s, auto good, auto bad )
                {
                    if ( s.compare("none") == 0 )
                        return good( Draw::None );
                    if ( s.compare("all") == 0 )
                        return good( Draw::All );
                    if ( s.compare("trace") == 0 )
                        return good( Draw::Trace );
                    return bad( cmd::BadContent, s + " is not a valid label type" );
                } ) ->
            add( "tracepoints", []( std::string s, auto good, auto bad )
                {
                    if ( s == "calls" )
                        return good( vm::AutoTrace::Calls );
                    if ( s == "none" )
                        return good( vm::AutoTrace::Nothing );
                    return bad( cmd::BadContent, s + " is nod a valid tracepoint specification" );
                } ) ->
            add( "paths", []( std::string s, auto good, auto )
                {
                    std::vector< std::string > out;
                    std::regex sep(":");
                    std::sregex_token_iterator it( s.begin(), s.end(), sep, -1 );
                    std::copy( it, std::sregex_token_iterator(), std::back_inserter( out ) );
                    return good( out );
                } ) ->
            add( "commasep", []( std::string s, auto good, auto )
                {
                    std::vector< std::string > out;
                    for ( auto x : brick::string::splitStringBy( s, "[\t ]*,[\t ]*" ) )
                        out.emplace_back( x );
                    return good( out );
                } );
    }

    auto commands()
    {
        auto v = validator();

        auto helpopts = cmd::make_option_set< Help >( v )
            .option( "[{string}]", &Help::_cmd, "print man to specified command"s );

        auto bcopts = cmd::make_option_set< WithBC >( v )
            .option( "[-D {string}|-D{string}]", &WithBC::_env, "add to the environment"s )
            .option( "[-C,{commasep}]", &WithBC::_ccOpts, "options passed to compiler compiler"s )
            .option( "[--autotrace {tracepoints}]", &WithBC::_autotrace, "insert trace calls"s )
            .option( "[-std={string}]", &WithBC::_std, "set the C or C++ standard to use"s )
            .option( "[--disable-static-reduction]", &WithBC::_disableStaticReduction,
                     "disable static (transformation based) state space reductions"s )
            .option( "[--lart {string}]", &WithBC::_lartPasses,
                     "run additional LART pass in the loader, can be specified multiple times" )
            .option( "[-o {string}|-o{string}]", &WithBC::_systemopts, "system options"s )
            .option( "[--vfslimit {mem}]", &WithBC::_vfsSizeLimit, "filesystem snapshot size limit (default 16 MiB)"s )
            .option( "[--capture {vfsdir}]", &WithBC::_vfs,
                "capture directory in form {dir}[:{follow|nofollow}[:{mount point}]]"s )
            .option( "[--stdin {file}]", &WithBC::_stdin, "capture file and pass it to OS as stdin for verified program" )
            .option( "{file}", &WithBC::_file, "the bitcode file to load"s,
                  cmd::OptionFlag::Required | cmd::OptionFlag::Final );

        using DrvOpt = cc::Options;
        auto ccdrvopts = cmd::make_option_set< DrvOpt >( v )
            .option( "[-c|--dont-link]", &DrvOpt::dont_link, "do not link"s );

        auto ccopts = cmd::make_option_set< Cc >( v )
            .options( ccdrvopts, &Cc::_drv )
            .option( "[-o {string}]", &Cc::_output, "the name of the output file"s )
            .option( "[-C,{commasep}]", &Cc::_passThroughFlags,
                     "options passed to compiler compiler (for compatibility with verify)"s )
            .option( "[{string}]", &Cc::_flags,
                     "any clang options including input file(s) to compile (C, C++, object, bitcode)"s );

        auto vrfyopts = cmd::make_option_set< Verify >( v )
            .option( "[--threads {int}|-T {int}]", &Verify::_threads, "number of threads to use"s )
            .option( "[--max-memory {int}]", &Verify::_max_mem, "max memory allowed to use [in MB]"s )
            .option( "[--max-time {int}]", &Verify::_max_time, "max time allowed to take [in sec]"s )
            .option( "[--no-counterexample]", &Verify::_no_counterexample,
                     "do not print counterexamples"s )
            .option( "[--report|-r]", &Verify::_report, "print a report to stdout, not just the result"s )
            .option( "[--max-backtrace-depth {int}]"s, &Verify::_backtraceMaxDepth,
                     "Maximum depth of error backtrace printed in the report [default = 10]" );

        auto drawopts = cmd::make_option_set< Draw >( v )
            .option( "[--distance {int}|-d {int}]", &Draw::_distance, "node distance"s )
            .option( "[--raw]", &Draw::_raw, "show hex dumps of heap objects"s )
            .option( "[--render {string}]", &Draw::_render, "command to execute on the dot output"s )
            .option( "[--labels {label}]", &Draw::_labels, "label all, none or only trace"s )
            .option( "[--bfs-layout]", &Draw::_bfs, "draw in bfs layout (levels)"s );

        auto dccopts = cmd::make_option_set< DivineCc >( v )
            .option( "[-o {string}]", &DivineCc::_output, "output file"s )
            .option( "[-c]", &DivineCc::_dontLink, "Compile or assemble the source files, but do not link."s )
            .option( "[--divinert-path {paths}]", &DivineCc::_libPaths, "paths to DIVINE runtime libraries (':' separated)"s )
            .option( "[{string}]", &DivineCc::_flags,
                     "any clang options including input file(s) to compile (C, C++, object, bitcode)"s );
        auto dldopts = cmd::make_option_set< DivineLd >( v )
            .option( "[-o {string}]", &DivineLd::_output, "output file"s )
            .option( "[-r|-i|--relocable]", &DivineLd::_incremental, "Generate incremental/relocable object file"s )
            .option( "[{string}]", &DivineLd::_flags, "any ld options including input file(s) to link"s );

        auto simopts = cmd::make_option_set< Sim >( v )
            .option( "[--batch]", &Sim::_batch, "execute in batch mode"s );

        auto parser = cmd::make_parser( v )
            .command< Verify >( &WithBC::_useropts, vrfyopts, bcopts )
            .command< Run >( &WithBC::_useropts, bcopts )
            .command< Sim >( &WithBC::_useropts, bcopts, simopts )
            .command< Draw >( drawopts, bcopts )
            .command< Info >( bcopts )
            .command< Cc >( ccopts )
            .command< DivineCc >( dccopts )
            .command< DivineLd >( dldopts )
            .command< Version >()
            .command< Help >( helpopts );
        return parser;
    }

    template< typename P >
    auto parse( P p )
    {
        if ( _args.size() >= 1 )
            if ( _args[0] == "--help" || _args[0] == "--version" )
                _args[0] = _args[0].substr( 2 );
        return p.parse( _args.begin(), _args.end() );
    }

    std::shared_ptr< Interface > resolve() override
    {
        if ( _batch )
            return Interface::resolve();
        else
            return std::make_shared< ui::Curses >( /* ... */ );
    }

    virtual int main() override
    {
        try {
            auto cmds = commands();
            auto cmd = parse( cmds );

            if ( cmd.empty()  )
            {
                Help().run( cmds );
                return 0;
            }

            cmd.match( [&]( Help &help )
                       {
                           help.run( cmds );
                       },
                       [&]( Command &c )
                       {
                           c.setup();
                           c.run();
                       } );
            return 0;
        }
        catch ( brick::except::Error &e )
        {
            std::cerr << "ERROR: " << e.what() << std::endl;
            exception( e );
            return e._exit;
        }
    }
};

}
}
