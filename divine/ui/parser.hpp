// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
 * (c) 2016 Viktória Vozárová <vikiv@mail.muni.cz>
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
    bool _batch, _check_files = true;
    brq::cmd_parser _parser;

    template< typename args_t >
    CLI( std::string_view argv_0, const args_t &args ) : _parser( argv_0, args, help() ) {}
    CLI( int argc, const char **argv ) : _batch( true ), _parser( argc, argv, help() ) {}

    std::string_view help()
    {
        return "DIVINE is an LLVM-based model checker. This means it can directly work with\n"
               "your C and C++ software. For small programs (those that fit in a single source\n"
               "file), you can use 'check' or 'verify' directly, without any intermediate steps.\n"
               "For larger projects, it is recommended that you use `dioscc` to build the project\n"
               "first: the resulting binaries can then be loaded into DIVINE, like this:\n\n"
               "    $ ./configure CC=dioscc\n"
               "    $ make # builds ./widget\n"
               "    $ divine check ./widget\n\n"
               "If `check` or `verify` find a problem in your program, they will print basic info\n"
               "about the error and a more detailed report into a file. You can use the `sim`\n"
               "command to analyse the detailed report, like this:\n\n"
               "    $ divine sim --load-report widget.report";
    }

    template< typename... extra_cmds >
    auto parse()
    {
        return _parser.parse< verify, check, exec, sim, draw, info, cc, version, ltlc, extra_cmds... >();
    }

    auto commands()
    {
        auto v = validator();

        auto helpopts = cmd::make_option_set( v )
            .option( "[{string}]", &Help::_cmd, "print man to specified command"s );

        auto bcopts = cmd::make_option_set( v )
            .option( "[-D {string}|-D{string}]", &WithBC::_env, "add to the environment"s )
            .option( "[-C,{commasep}]", &WithBC::_ccOpts, "pass additional options to the compiler"s )
            .option( "[--autotrace {tracepoints}]", &WithBC::_bc_opts, &mc::BCOptions::autotrace,
                     "insert trace calls"s )
            .option( "[--leakcheck {leakpoints}]", &WithBC::_bc_opts, &mc::BCOptions::leakcheck,
                     "insert leak checks"s )
            .option( "[--sequential]", &WithBC::_bc_opts, &mc::BCOptions::sequential,
                     "disable support for threading"s )
            .option( "[--synchronous]", &WithBC::_bc_opts, &mc::BCOptions::synchronous,
                     "enable synchronous mode"s )
            .option( "[-std={string}]", &WithBC::_std, "set the C or C++ standard to use"s )
            .option( "[--disable-static-reduction]", &WithBC::_bc_opts,
                     &mc::BCOptions::disable_static_reduction,
                     "disable static (transformation based) state space reductions"s )
            .option( "[--relaxed-memory {string}]", &WithBC::_bc_opts, &mc::BCOptions::relaxed,
                     "enable relaxed memory semantics (tso|pso[:N]) where N is size of buffers"s )
            .option( "[--lart {string}]", &WithBC::_bc_opts, &mc::BCOptions::lart_passes,
                     "run an additional LART pass in the loader" )
            .option( "[-o {string}|-o{string}]", &WithBC::_systemopts, "system options"s )
            .option( "[--vfslimit {mem}]", &WithBC::_vfsSizeLimit,
                     "filesystem snapshot size limit (default 16 MiB)"s )
            .option( "[--capture {vfsdir}]", &WithBC::_vfs,
                     "capture directory in form {dir}[:{follow|nofollow}[:{mount point}]]"s )
            .option( "[--stdin {file}]", &WithBC::_stdin,
                     "capture file and pass it to OS as stdin for verified program" )
            .option( "[--symbolic]", &WithBC::_bc_opts, &mc::BCOptions::symbolic,
                     "use semi-symbolic data representation"s )
            .option( "[--svcomp]", &WithBC::_bc_opts, &mc::BCOptions::svcomp,
                     "run SV-COMP specific program transformations"s )
            .option( "[--dump-bc {string}]", &WithBC::_dump_bc,
                     "dump the final pre-processed bitcode"s )
            .option( "[--dios-config {string}]", &WithBC::_bc_opts, &mc::BCOptions::dios_config,
                     "which DiOS configuration to use"s )
            .option( "[-l{string}|-l {string}]", &WithBC::_linkLibs,
                     "link in a library, e.g. -lm for libm"s )
            .option( "{file}", &WithBC::_bc_opts, &mc::BCOptions::input_file, "the bitcode file to load"s,
                  cmd::OptionFlag::Required | cmd::OptionFlag::Final );

        auto ltlcopts = cmd::make_option_set( v )
            .option( "[--automaton {string}|-a {string}]", &Ltlc::_automaton, "text file containing TGBA in HOA format"s )
            .option( "[--formula {string}|-f {string}]", &Ltlc::_formula, "LTL formula"s )
            .option( "[--negate]", &Ltlc::_negate, "compute automaton of negation of given formula"s )
            .option( "[--output {string}|-o {string}]", &Ltlc::_output, "name of the file"s )
            .option( "[--system {string}|-s {string}]", &Ltlc::_system, "system to be verified"s );

        auto ccopts = cmd::make_option_set( v )
            .option( "[-c|--dont-link]", &Cc::_opts, &cc::Options::dont_link, "do not link"s )
            .option( "[-o {string}]", &Cc::_output, "the name of the output file"s )
            .option( "[-C,{commasep}]", &Cc::_passThroughFlags,
                     "pass additional options to the compiler"s )
            .option( "[{string}]", &Cc::_flags,
                     "any clang options or input files (C, C++, object, bitcode)"s );

        auto vrfyopts = cmd::make_option_set( v )
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

        auto drawopts = cmd::make_option_set( v )
            .option( "[--distance {int}|-d {int}]", &Draw::_distance, "maximum node distance"s )
            .option( "[--render {string}]", &Draw::_render, "the command to process the dot source"s );

        auto runopts = cmd::make_option_set( v )
            .option( "[--virtual]", &Exec::_virtual, "simulate system calls instead of executing them"s)
            .option( "[--trace]", &Exec::_trace, "trace instructions"s);

        auto simopts = cmd::make_option_set( v )
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

    virtual int main() override try
    {
        auto cmd = parse();

        cmd.match( [&]( brq::cmd_help &help )
                   {
                       help.run();
                   },
                   [&]( command &c )
                   {
                       c.setup();
                       c.run();
                       c.cleanup();
                   } );
        return 0;
    }
    catch ( brq::error &e )
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        exception( e );
        return e._exit;
    }
};

}
