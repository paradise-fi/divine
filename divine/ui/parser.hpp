// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
 * (c) 2016 Viktória Vozárová <>
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
#include <divine/ui/cli.hpp>

namespace divine::ui
{

struct CLI : Interface
{
    bool _batch;
    std::vector< std::string > _args;

    CLI( int argc, const char **argv ) :
        _batch( true ), _args( cmd::from_argv( argc, argv ) )
    { }

    CLI( std::vector< std::string > args ) :
        _batch( true ), _args( args )
    { }

    auto validator()
    {
        auto file = []( std::string s, auto good, auto bad )
        {
            if ( s[0] == '-' ) /* FIXME! zisit, kde sa to rozbije */
                return bad( cmd::BadFormat, "file must not start with -" );
            if ( !brick::fs::access( s, F_OK ) )
                return bad( cmd::BadContent, "file " + s + " does not exist");
            if ( !brick::fs::access( s, R_OK ) )
                return bad( cmd::BadContent, "file " + s + " is not readable");
            return good( s );
        };

        auto vfsdir = []( std::string s, auto good, auto bad )
        {
            WithBC::VfsDir dir;
            dir.followSymlink = true;
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
        };

        auto mem = []( std::string s, auto good, auto bad )
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
        };

        auto repfmt = []( std::string s, auto good, auto bad )
        {
            if ( s.compare("none") == 0 )
                return good( Report::None );
            if ( s.compare("yaml-long") == 0 )
                return good( Report::YamlLong );
            if ( s.compare("yaml") == 0 )
                return good( Report::Yaml );
            return bad( cmd::BadContent, s + " is not a valid report format" );
        };

        auto label = []( std::string s, auto good, auto bad )
        {
            if ( s.compare("none") == 0 )
                return good( Draw::None );
            if ( s.compare("all") == 0 )
                return good( Draw::All );
            if ( s.compare("trace") == 0 )
                return good( Draw::Trace );
            return bad( cmd::BadContent, s + " is not a valid label type" );
        };

        auto tracepoints = []( std::string s, auto good, auto bad )
        {
            if ( s == "calls" )
                return good( mc::AutoTrace::Calls );
            if ( s == "none" )
                return good( mc::AutoTrace::Nothing );
            return bad( cmd::BadContent, s + " is not a valid tracepoint specification" );
        };

        auto paths = []( std::string s, auto good, auto )
        {
            std::vector< std::string > out;
            std::regex sep(":");
            std::sregex_token_iterator it( s.begin(), s.end(), sep, -1 );
            std::copy( it, std::sregex_token_iterator(), std::back_inserter( out ) );
            return good( out );
        };

        auto commasep = []( std::string s, auto good, auto )
        {
            std::vector< std::string > out;
            for ( auto x : brick::string::splitStringBy( s, "[\t ]*,[\t ]*" ) )
                out.emplace_back( x );
            return good( out );
        };

        auto result = []( std::string s, auto good, auto bad )
        {
            if ( s == "valid" ) return good( mc::Result::Valid );
            if ( s == "error" ) return good( mc::Result::Error );
            if ( s == "boot-error" ) return good( mc::Result::BootError );
            return bad( ui::cmd::BadContent, s + " is not a valid result specification" );
        };

        return cmd::make_validator()->
            add( "file", file )->
            add( "vfsdir", vfsdir )->
            add( "mem", mem )->
            add( "repfmt", repfmt )->
            add( "label", label )->
            add( "tracepoints", tracepoints )->
            add( "paths", paths )->
            add( "result", result )->
            add( "commasep", commasep );
    }

    auto commands()
    {
        auto v = validator();

        auto helpopts = cmd::make_option_set< Help >( v )
            .option( "[{string}]", &Help::_cmd, "print man to specified command"s );

        auto bcopts = cmd::make_option_set< WithBC >( v )
            .option( "[-D {string}|-D{string}]", &WithBC::_env, "add to the environment"s )
            .option( "[-C,{commasep}]", &WithBC::_ccOpts, "pass additional options to the compiler"s )
            .option( "[--autotrace {tracepoints}]", &WithBC::_autotrace, "insert trace calls"s )
            .option( "[--sequential]", &WithBC::_sequential, "disable support for threading"s )
            .option( "[--synchronous]", &WithBC::_synchronous, "enable synchronous mode"s )
            .option( "[-std={string}]", &WithBC::_std, "set the C or C++ standard to use"s )
            .option( "[--disable-static-reduction]", &WithBC::_disableStaticReduction,
                     "disable static (transformation based) state space reductions"s )
            .option( "[--relaxed-memory {string}]", &WithBC::_relaxed,
                     "enable relaxed memory semantics (tso|pso[:N]) where N is size of buffers"s )
            .option( "[--lart {string}]", &WithBC::_lartPasses,
                     "run an additional LART pass in the loader" )
            .option( "[-o {string}|-o{string}]", &WithBC::_systemopts, "system options"s )
            .option( "[--vfslimit {mem}]", &WithBC::_vfsSizeLimit,
                     "filesystem snapshot size limit (default 16 MiB)"s )
            .option( "[--capture {vfsdir}]", &WithBC::_vfs,
                     "capture directory in form {dir}[:{follow|nofollow}[:{mount point}]]"s )
            .option( "[--stdin {file}]", &WithBC::_stdin,
                     "capture file and pass it to OS as stdin for verified program" )
            .option( "[--symbolic]", &WithBC::_symbolic,
                     "use semi-symbolic data representation"s )
            .option( "[--svcomp]", &WithBC::_svcomp,
                     "run SV-COMP specific program transformations"s )
            .option( "[--dump-bc {string}]", &WithBC::_dump_bc,
                     "dump the final pre-processed bitcode"s )
            .option( "[-l{string}|-l {string}]", &WithBC::_linkLibs,
                     "link in a library, e.g. -lm for libm"s )
            .option( "{file}", &WithBC::_file, "the bitcode file to load"s,
                  cmd::OptionFlag::Required | cmd::OptionFlag::Final );

        using DrvOpt = cc::Options;
        auto ccdrvopts = cmd::make_option_set< DrvOpt >( v )
            .option( "[-c|--dont-link]", &DrvOpt::dont_link, "do not link"s );

        auto ltlcopts = cmd::make_option_set< Ltlc >( v )
            .option( "[--formula {string}|-f {string}]", &Ltlc::_formula, "formula LTL"s )
            .option( "[--negate]", &Ltlc::_negate, "automaton of negation of f"s )
            .option( "[--output {string}|-o {string}]", &Ltlc::_output, "name of the file"s )
            .option( "[--system {string}|-s {string}]", &Ltlc::_system, "system to be verified"s );

        auto ccopts = cmd::make_option_set< Cc >( v )
            .options( ccdrvopts, &Cc::_opts )
            .option( "[-o {string}]", &Cc::_output, "the name of the output file"s )
            .option( "[-C,{commasep}]", &Cc::_passThroughFlags,
                     "pass additional options to the compiler"s )
            .option( "[{string}]", &Cc::_flags,
                     "any clang options or input files (C, C++, object, bitcode)"s );

        auto vrfyopts = cmd::make_option_set< Verify >( v )
            .option( "[--threads {int}|-T {int}]", &Verify::_threads, "number of threads to use"s )
            .option( "[--max-memory {mem}]", &Verify::_max_mem, "limit memory use"s )
            .option( "[--max-time {int}]", &Verify::_max_time, "maximum allowed run time in seconds"s )
            .option( "[--liveness]", &Verify::_liveness, "enables verification of liveness properties"s )
            .option( "[--solver {string}]", &Verify::_solver, "the solver backend to use"s )
            .option( "[--report {repfmt}|-r {repfmt}]", &Verify::_report,
                     "print a report (yaml, yaml-long or none)"s )
            .option( "[--no-report-file]", &Verify::_no_report_file, "skip creation of a report file"s )
            .option( "[--report-filename {string}]", &Verify::_report_filename,
                     "write the report into a given file"s )
            .option( "[--report-unique]", &Verify::_report_unique,
                     "ensure the report filename is unique"s )
            .option( "[--num-callers {int}]"s, &Verify::_num_callers,
                     "the number of frames to print in a backtrace [default = 10]" );

        auto drawopts = cmd::make_option_set< Draw >( v )
            .option( "[--distance {int}|-d {int}]", &Draw::_distance, "maximum node distance"s )
            .option( "[--render {string}]", &Draw::_render, "the command to process the dot source"s );

        auto runopts = cmd::make_option_set< Exec >( v )
            .option( "[--virtual]", &Exec::_virtual, "simulate system calls instead of executing them"s)
            .option( "[--trace]", &Exec::_trace, "trace instructions"s);

        auto simopts = cmd::make_option_set< Sim >( v )
            .option( "[--batch]", &Sim::_batch, "execute in batch mode"s )
            .option( "[--load-report]", &Sim::_load_report, "load a verify report"s )
            .option( "[--skip-init]", &Sim::_skip_init, "do not load ~/.divine/sim.init"s );

        auto parser = cmd::make_parser( v )
            .command< Verify >( &WithBC::_useropts, vrfyopts, bcopts )
            .command< Check >( &WithBC::_useropts, vrfyopts, bcopts )
            .command< Exec >( &WithBC::_useropts, bcopts, runopts )
            .command< Sim >( &WithBC::_useropts, bcopts, simopts )
            .command< Draw >( drawopts, bcopts )
            .command< Info >( bcopts )
            .command< Cc >( ccopts )
            .command< Version >()
            .command< Help >( helpopts )
            .command< Ltlc >( ltlcopts );
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
                           c.cleanup();
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
