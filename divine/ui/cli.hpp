#pragma once

#include <divine/vm/bitcode.hpp>
#include <divine/cc/options.hpp>

#include <divine/ui/common.hpp>
#include <divine/ui/curses.hpp>
#include <divine/ui/die.hpp>

#include <brick-cmd>
#include <brick-fs>

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
    std::string _file;
    std::vector< std::string > _env;
    std::vector< std::string > _useropts;
    vm::AutoTraceFlags _autotrace;

    std::shared_ptr< vm::BitCode > _bc;
    void setup();
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

struct Verify : WithBC
{
    int _max_mem = 256; // MB
    int _max_time = 100;  // seconds
    int _jobs = 1;
    bool _no_counterexample = false;
    std::string _report;
    std::string _statistics;

    void print_args()
    {
        std::cerr << "Verify model " << _file << " with given options:" << std::endl;
        std::cerr << "Max. memory allowed [MB]: " << _max_mem << std::endl;
        std::cerr << "Max. time allowed [sec]: " << _max_time << std::endl;
        std::cerr << "Number of jobs: " << _jobs << std::endl;
        if ( _no_counterexample )
            std::cerr << "Do not print counter example." << std::endl;
        else
            std::cerr << "If program fails, print counter example." << std::endl;
        std::cerr << "Give report in format: " << _report << std::endl;
        std::cerr << "Give statistics in format: " << _statistics << std::endl;
    }

    void run();
};

struct Run : WithBC { void run(); };
struct Sim : WithBC { void run(); };

struct Draw : WithBC
{
    int _number = 0;
    int _distance = 0;
    enum { All, None, Trace } _labels = None;
    bool _bfs = false;
    std::string _render = std::string( "dot -Tx11" );

    void print_args()
    {
        std::cerr << "(" << _number << ") Draw model " << _file << " with given options:" << std::endl;
        if ( _labels == All )
            std::cerr << "Draw all node labels." << std::endl;
        if ( _labels == None )
            std::cerr << "Do not draw any labels." << std::endl;
        if ( _labels == Trace )
            std::cerr << "Draw only trace labels." << std::endl;
        std::cerr << "Node distance [cm]: " <<  _distance << std::endl;
        if ( _bfs )
            std::cerr << "Draw as BFS." << std::endl;
        std::cerr << "Render with " << _render << std::endl;
    }

    void run();
};

struct Cc : Command
{
    cc::Options _drv;
    std::vector< std::string > _files, _flags, _inc, _sysinc;
    std::string _precompiled;
    std::string _output;

    void run();
};

struct Info   : WithBC
{
    void run() { NOT_IMPLEMENTED(); }
};

namespace {

std::vector< std::string > fromArgv( int argc, const char **argv )
{
    std::vector< std::string > args;
    std::copy( argv + 1, argv + argc, std::back_inserter( args ) );
    return args;
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
                } );
    }

    auto commands()
    {
        auto v = validator();

        auto helpopts = cmd::make_option_set< Help >( v )
            .option( "[{string}]", &Help::_cmd, "print man to specified command"s );

        auto bcopts = cmd::make_option_set< WithBC >( v )
            .option( "[-D {string}|-D{string}]", &WithBC::_env, "add to the environment"s )
            .option( "[--autotrace {tracepoints}]", &WithBC::_autotrace, "insert trace calls"s )
            .option( "{file}", &WithBC::_file, "the bitcode file to load"s,
                  cmd::OptionFlag::Required | cmd::OptionFlag::Final );

        using DrvOpt = cc::Options;
        auto ccdrvopts = cmd::make_option_set< DrvOpt >( v )
            .option( "[--precompiled {file}]", &DrvOpt::precompiled,
                     "path to a prebuilt libdivinert.bc"s )
            .option( "[-j {int}]", &DrvOpt::num_threads, "number of jobs"s )
            .option( "[-c|--dont-link]", &DrvOpt::dont_link, "do not link"s )
            .option( "[--libraries-only]", &DrvOpt::libs_only, "build libdivinert.bc for later use"s );

        auto ccopts = cmd::make_option_set< Cc >( v )
            .options( ccdrvopts, &Cc::_drv )
            .option( "[-o {string}]", &Cc::_output, "the name of the output file"s )
            .option( "[-{string}]", &Cc::_flags, "any cc1 options"s )
            .option( "[-I{string}|-I {string}]", &Cc::_inc,
                    "add the specified directory to the search path for include files"s )
            .option( "[-isystem{string}|-isystem {string}]", &Cc::_sysinc,
                    "like -I but searched later (along with system include dirs)"s )
            .option( "[{file}]", &Cc::_files, "input file(s) to compile (C or C++)"s );

        auto vrfyopts = cmd::make_option_set< Verify >( v )
            .option( "[--max-memory {int}]", &Verify::_max_mem, "max memory allowed to use [in MB]"s )
            .option( "[--max-time {int}]", &Verify::_max_time, "max time allowed to take [in sec]"s )
            .option( "[--no-counterexample]", &Verify::_no_counterexample,
                     "do not print counterexamples"s )
            .option( "[--report {string}]", &Verify::_report, "print a report with given options"s )
            .option( "[--statistics {string}]", &Verify::_statistics,
                     "print statistics with given options"s );

        auto drawopts = cmd::make_option_set< Draw >( v )
            .option( "[--distance {int}|-d {int}]", &Draw::_distance, "node distance"s )
            .option( "[--labels {label}]", &Draw::_labels, "label all, none or only trace"s )
            .option( "[--bfs-layout]", &Draw::_bfs, "draw in bfs layout (levels)"s );

        auto parser = cmd::make_parser( v )
            .command< Verify >( vrfyopts, bcopts )
            .command< Run >( &WithBC::_useropts, bcopts )
            .command< Sim >( &WithBC::_useropts, bcopts )
            .command< Draw >( drawopts, bcopts )
            .command< Info >( bcopts )
            .command< Cc >( ccopts )
            .command< Help >( helpopts );
        return parser;
    }

    template< typename P >
    auto parse( P p )
    {
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
