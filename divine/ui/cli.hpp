#pragma once

#include <divine/vm/bitcode.hpp>
#include <divine/vm/run.hpp>

#include <divine/ui/common.hpp>
#include <divine/ui/curses.hpp>
#include <brick-cmd>
#include <brick-fs>
#include <brick-llvm>

namespace divine {
namespace ui {

namespace cmd = brick::cmd;

struct Command
{
    virtual void run() = 0;
    virtual void setup() {}
};

struct WithBC : Command
{
    std::string _file;
    std::vector< std::string > _env;

    std::shared_ptr< vm::BitCode > _bc;
    void setup()
    {
        _bc = std::make_shared< vm::BitCode >( _file );
    }
};

struct Help
{
    std::string _cmd = std::string("");

    template< typename P >
    void run( P cmds )
    {
        std::string description = cmd::get_desc_by_name( cmds, _cmd );
        if ( description.compare( "" ) == 0)
        {
            if ( _cmd.compare("") != 0 )
            {
                std::cerr << "Command '" << _cmd << "' not recognized.\n\n";
                _cmd = std::string( "" );
            }
        }
        else
            std::cerr << description << std::endl;
        if ( _cmd.compare( "" ) == 0 )
        {
            std::cerr << "To print details about specific command write 'divine help [command]'.\n\n";
            std::cerr << cmd::describe( cmds ) << std::endl;
        }
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

    void run()
    {
        print_args();
    }
};

struct Run : WithBC
{
    void run() { vm::Run( _bc, _env ).run(); }
};

struct Draw   : WithBC
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

    void run()
    {
        print_args();
    }
};

struct Cc     : Command
{
    std::string _file;
    std::string _precompiled;
    std::vector< std::string > _flags;
    std::vector< std::string > _mics;
    bool _libraries_only = false;
    bool _fairness = false;
    bool _dont_link = false;
    int _jobs = 1;

    void print_args()
    {
        std::cerr << "File to compile: " << _file << std::endl;
        std::cerr << "Number of jobs: " << _jobs << std::endl;
        std::cerr << "Precompiled: " << _precompiled << std::endl;

        std::cerr << "Flags:";
        for ( std::string flag : _flags )
            std::cerr << " " << flag;
        std::cerr << std::endl;

        std::cerr << "Other options:";
        for ( std::string opt : _mics )
            std::cerr << " " << opt;
        std::cerr << std::endl;

        if (_libraries_only)
            std::cerr << "Libraries only." << std::endl;
        if (_fairness)
            std::cerr << "Fairness." << std::endl;
        if (_dont_link)
            std::cerr << "Do not link" << std::endl;
    }

    void run()
    {
        print_args();
    }
};

struct Info   : WithBC
{
    void run() { NOT_IMPLEMENTED(); }
};

std::vector< std::string > fromArgv( int argc, const char **argv ) {
    std::vector< std::string > args;
    std::copy( argv + 1, argv + argc, std::back_inserter( args ) );
    return args;
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
                });
    }

    // @return ParserT
    auto commands()
    {
        auto v = validator();

        auto helpopts = cmd::make_option_set< Help >( v )
                .add( "[{string}]", &Help::_cmd, std::string( "print man to specified command" ) );

        auto bcopts = cmd::make_option_set< WithBC >( v )
                .add( "[-D {string}]", &WithBC::_env, std::string( "add to the environment" ) )
                .add( "{file}", &WithBC::_file, std::string( "the bitcode file to load" ) );

        auto ccopts = cmd::make_option_set< Cc >( v )
                .add( "[-j {int}]", &Cc::_jobs, std::string( "number of jobs" ) )
                .add( "[--precompiled={string}]", &Cc::_precompiled, std::string( "no idea what this is" ) ) // MIXED
                .add( "[--fair]", &Cc::_fairness, std::string( "fairness" ) )
                .add( "[--libraries-only]", &Cc::_libraries_only, std::string( "compile only libraries" ) )
                .add( "[--dont-link]", &Cc::_dont_link, std::string( "do not link" ) )
                .add( "[-o {string}+]", &Cc::_flags, std::string( "compiler flags" ) )
                .add( "[-{string}+]", &Cc::_mics, std::string( "any other arbitrary options" ) ) // MIXED
                .add( "{file}", &Cc::_file, std::string( "input file to compile (of type .c, .cpp, .hpp ...)" ), true );

        auto vrfyopts = cmd::make_option_set< Verify >( v )
                .add( "[--max-memory {int}]", &Verify::_max_mem, std::string( "max memory allowed to use [in MB]" ) ) // MIXED
                .add( "[--max-time {int}]", &Verify::_max_time, std::string( "max time allowed to take [in sec]") ) // MIXED
                .add( "[--no-counterexample]", &Verify::_no_counterexample, std::string( "do not print any counter example, if program fails") )
                .add( "[--report {string}]", &Verify::_report, std::string( "print report with given options" ) ) // MIXED
                .add( "[--statistics {string}]", &Verify::_statistics, std::string( "print statistics with given options" ) ); // MIXED

        auto drawopts = cmd::make_option_set< Draw >( v )
                .add( "[--distance {int}|-d {int}]", &Draw::_distance, std::string( "node distance" ) ) // MIXED
                .add( "[--labels {label}]", &Draw::_labels, std::string( "label all, none or only trace" ) ) // MIXED
                .add( "[--bfs-layout]", &Draw::_bfs, std::string( "draw in bfs layout (levels)" ) )
                .add( "[{int}]", &Draw::_number, std::string("any random number") );

        auto parser = cmd::make_parser( v )
                .add< Verify >( vrfyopts, bcopts )
                .add< Run >( bcopts )
                .add< Draw >( drawopts, bcopts )
                .add< Info >( bcopts )
                .add< Cc >( ccopts )
                .add< Help >( helpopts );
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
        auto cmds = commands();
        auto cmd = parse( cmds );

        if ( cmd.empty()  )
        {
            Help help;
            help.run( cmds );
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
};

}
}
