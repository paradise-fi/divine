// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>
#include <wibble/test.h> // for assert
#include <queue>
#include <iostream>

#include <wibble/sys/thread.h>
#include <wibble/sys/mutex.h>
#include <wibble/commandline/parser.h>
#include <wibble/string.h>
#include <wibble/sfinae.h>
#include <wibble/sys/fs.h>

#include <divine/config.h>
#include <divine/generator/dummy.h>
#include <divine/generator/legacy.h>
#include <divine/generator/custom.h>

#include <divine/reachability.h>
#include <divine/owcty.h>
#include <divine/metrics.h>

#include <divine/report.h>

#include <tools/combine.h>

#include <sys/resource.h>

using namespace divine;
using namespace wibble;
using namespace commandline;

struct Preferred {};
struct NotPreferred { NotPreferred( Preferred ) {} };

Report *report = 0;

void handler( int s ) {
    if ( report ) {
        report->signal( s );
        report->final( std::cout );
    }
    signal( s, SIG_DFL );
    raise( s );
}

struct Main {
    Config config;

    Engine *cmd_reachability, *cmd_owcty, *cmd_ndfs, *cmd_map, *cmd_verify,
        *cmd_metrics;
    OptionGroup *common;
    BoolOption *o_verbose, *o_pool, *o_noCe, *o_report;
    IntOption *o_workers, *o_mem;
    StringOption *o_trail;

    bool dummygen;

    int argc;
    const char **argv;
    commandline::StandardParserWithMandatoryCommand opts;

    Combine combine;

    Main( int _argc, const char **_argv )
        : dummygen( false ), argc( _argc ), argv( _argv ),
          opts( argv[ 0 ], DIVINE_VERSION, 1, "DiVinE Team <divine@fi.muni.cz>" ),
          combine( opts, argc, argv )
    {
        setupSignals();
        setupCommandline();
        parseCommandline();

        if ( m_run == RunCombine ) {
            combine.main();
            return;
        }

        Report rep( config );
        report = &rep;
        rep.start();
        rep.finished( selectAlgorithm() );
        rep.final( std::cout );
    }

    void die( std::string bla ) __attribute__((noreturn))
    {
        std::cerr << bla << std::endl;
        exit( 1 );
    }

    void FIXME( std::string x ) __attribute__((noreturn)) { die( x ); }

    void verbose( std::string bla )
    {
        if ( config.verbose() )
            std::cerr << bla << std::endl;
    }

    void setupSignals() 
    {
        for ( int i = 0; i <= 32; ++i ) {
            if ( i == SIGCHLD || i == SIGWINCH || i == SIGURG )
                continue;
            signal( i, handler );
        }
    }

    void setupCommandline()
    {
        opts.usage = "<command> [command-specific options] <input file>";
        opts.description = "A Parallel Explicit-State LTL Model Checker";

        cmd_metrics = opts.addEngine( "metrics",
                                      "<input>",
                                      "state space metrics");
        cmd_reachability = opts.addEngine( "reachability",
                                           "<input>",
                                           "reachability analysis");
        cmd_verify = opts.addEngine( "verify",
                                     "<input>",
                                     "verification" );
        cmd_owcty = opts.addEngine( "owcty", "<input>",
                                    "one-way catch them young cycle detection");
        cmd_ndfs = opts.addEngine( "nested-dfs", "<input>",
                                   "nested dfs cycle detection" );
        cmd_map = opts.addEngine( "map", "<input>",
                                  "maximum accepting predecessor "
                                  "cycle detection" );

        common = opts.createGroup( "Common Options" );

        o_verbose = common->add< BoolOption >(
            "verbose", 'v', "verbose", "", "more verbose operation" );
        o_report = common->add< BoolOption >(
            "report", 'r', "report", "", "output standardised report" );

        o_workers = common->add< IntOption >(
            "workers", 'w', "workers", "",
            "number of worker threads (default: 2)" );
        o_workers ->setValue( 2 );

        o_mem = common->add< IntOption >(
            "max-memory", '\0', "max-memory", "",
            "maximum memory to use in MB (default: 0 = unlimited)" );

        o_pool = common->add< BoolOption >(
            "disable-pool", '\0', "disable-pool", "",
            "disable pooled allocation (use HOARD for all allocation)" );

        o_noCe = common->add< BoolOption >(
            "no-counterexample", 'n', "no-counterexample", "",
            "disable counterexample generation" );

        o_trail = common->add< StringOption >(
            "trail", 't', "trail", "",
            "file to output trail to (default: input-file.trail)" );

        opts.add( common );
    }

    enum { RunCombine, RunMetrics, RunReachability, RunNdfs,
           RunMap, RunOwcty } m_run;

    void parseCommandline()
    {
        std::string input;

        try {
            if ( opts.parse( argc, argv ) )
                exit( 0 ); // built-in command executed

            if ( !opts.hasNext() ) {
                std::cerr
                    << "WARNING: no input file specified, using dummy generator"
                    << std::endl;
                dummygen = true;
            }
            input = opts.next();

        } catch( wibble::exception::BadOption &e ) {
            die( e.fullInfo() );
        }

        if ( !opts.foundCommand() )
            die( "FATAL: no command specified" );

        if ( opts.foundCommand() == combine.cmd_combine ) {
            m_run = RunCombine;
            return;
        }

        config.setWorkers( o_workers->intValue() );
        config.setInput( input );
        config.setVerbose( o_verbose->boolValue() );
        config.setReport( o_report->boolValue() );
        config.setGenerateCounterexample( !o_noCe->boolValue() );

        if ( o_mem->intValue() != 0 ) {
            if ( o_mem->intValue() < 0 ) {
                die( "FATAL: cannot have negative memory limit" );
            }
            if ( o_mem->intValue() < 16 ) {
                die( "FATAL: we really need at least 16M of memory" );
            }
            struct rlimit limit;
            limit.rlim_cur = limit.rlim_max = o_mem->intValue() * 1024 * 1024;
            if (setrlimit( RLIMIT_AS, &limit ) != 0) {
                int err = errno;
                std::cerr << "WARNING: Could not set memory limit to "
                          << o_mem->intValue() << "MB. System said: "
                          << strerror(err) << std::endl;
            }
        }

        if ( o_trail->stringValue() == "" ) {
            std::string t = std::string( input, 0,
                                         input.rfind( '.' ) );
            t += ".trail";
            config.setTrailFile( t );
        } else
            config.setTrailFile( o_trail->stringValue() );

        if ( !dummygen && access( input.c_str(), R_OK ) )
            die( "FATAL: cannot open input file " + input + " for reading" );

        if ( opts.foundCommand() == cmd_verify ) {
            std::string inf = fs::readFile( config.input() );
            if ( inf.find( "system async property" ) != std::string::npos ||
                 inf.find( "system sync property" ) != std::string::npos ) {
                // we have a property automaton --> LTL
                if ( config.workers() > 1 ) {
                    m_run = RunOwcty;
                } else {
                    m_run = RunNdfs;
                }
            } else {
                m_run = RunReachability; // no property
            }
        } else if ( opts.foundCommand() == cmd_ndfs )
            m_run = RunNdfs;
        else if ( opts.foundCommand() == cmd_owcty )
            m_run = RunOwcty;
        else if ( opts.foundCommand() == cmd_reachability )
            m_run = RunReachability;
        else if ( opts.foundCommand() == cmd_map )
            m_run = RunMap;
        else if ( opts.foundCommand() == cmd_metrics )
            m_run = RunMetrics;
        else
            die( "FATAL: Internal error in commandline parser." );

        if ( config.verbose() ) {
            std::cerr << " === configured options ===" << std::endl;
            config.dump( std::cerr );
        }
    }

    Result selectAlgorithm()
    {
        switch ( m_run ) {
            case RunReachability:
                config.setAlgorithm( "Reachability" );
                return selectGraph< algorithm::Reachability >();
            case RunMetrics:
                config.setAlgorithm( "Metrics" );
                return selectGraph< algorithm::Metrics >();
            case RunOwcty:
                config.setAlgorithm( "OWCTY" );
                return selectGraph< algorithm::Owcty >();
            case RunMap:
                config.setAlgorithm( "MAP" );
                FIXME( "Sorry, MAP currently not implemented." );
            case RunNdfs:
                config.setAlgorithm( "NestedDFS" );
                FIXME( "Sorry, Nested DFS currently not implemented." );
            default:
                die( "FATAL: Internal error choosing algorithm." );
        }
    }

    template< template< typename > class Algorithm >
    Result selectGraph()
    {
        if ( str::endsWith( config.input(), ".dve" ) ) {
            config.setGenerator( "DiVinE" );
            return run< Algorithm< generator::NDve > >();
        } else if ( str::endsWith( config.input(), ".b" ) ) {
            config.setGenerator( "NIPS" );
            return run< Algorithm< generator::NBymoc > >();
        } else if ( str::endsWith( config.input(), ".so" ) ) {
            config.setGenerator( "Custom" );
            return run< Algorithm< generator::Custom > >();
        } else if ( dummygen ) {
            config.setGenerator( "Dummy" );
            return run< Algorithm< generator::Dummy > >();
        } else
	    die( "FATAL: Unknown input file extension." );
    }

    template< typename A >
    Result run() {
        A alg( &config );

        // TBD. This does not belong here. It is, however, convenient.
        alg.domain.mpi.start();
        report->mpiInfo( alg.domain.mpi );

        return alg.run();
    }

};


int main( int argc, const char **argv )
{
    Main m( argc, argv );
    return 0;
}
