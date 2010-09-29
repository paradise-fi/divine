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

// The algorithms.
#include <divine/algorithm/reachability.h>
#include <divine/algorithm/owcty.h>
#include <divine/algorithm/metrics.h>
#include <divine/algorithm/map.h>
#include <divine/algorithm/nested-dfs.h>

#include <divine/porcp.h>

#include <divine/report.h>

#include <tools/combine.h>
#include <tools/compile.h>

#ifdef POSIX
#include <sys/resource.h>
#endif

using namespace divine;
using namespace wibble;
using namespace commandline;

Report *report = 0;

void handler( int s ) {
    signal( s, SIG_DFL );
    Output::output().cleanup();
    if ( report ) {
        report->signal( s );
        report->final( std::cout );
    }
    raise( s );
}

struct Main {
    Config config;
    Output *output;

    Engine *cmd_reachability, *cmd_owcty, *cmd_ndfs, *cmd_map, *cmd_verify,
        *cmd_metrics, *cmd_compile;
    OptionGroup *common;
    BoolOption *o_verbose, *o_pool, *o_noCe, *o_dispCe, *o_report, *o_dummy, *o_statistics;
    BoolOption *o_por;
    BoolOption *o_curses;
    IntOption *o_workers, *o_mem, *o_time, *o_initable;
    StringOption *o_trail;

    bool dummygen;
    bool statistics;

    int argc;
    const char **argv;
    commandline::StandardParserWithMandatoryCommand opts;

    Combine combine;
    Compile compile;

    ~Main() {
        delete Output::_output; // to clean up after ourselves
    }

    Main( int _argc, const char **_argv )
        : dummygen( false ), statistics( false ), argc( _argc ), argv( _argv ),
          opts( "DiVinE", versionString(), 1, "DiVinE Team <divine@fi.muni.cz>" ),
          combine( opts, argc, argv ),
          compile( opts )
    {
        Output::_output = makeStdIO( std::cerr );

        setupSignals();
        setupCommandline();
        parseCommandline();

        if ( m_noMC ) {
            noMC();
            return;
        }

        Report rep( config );
        report = &rep;
        Result res;

#ifdef PERFORMANCE
        if ( statistics )
            res = selectGraph< Statistics >();
        else
            res = selectGraph< NoStatistics >();
#else
        res = selectGraph< Statistics >();
#endif
        rep.finished( res );
        rep.final( std::cout );
    }

    static void die( std::string bla ) __attribute__((noreturn))
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
#ifdef POSIX
            if ( i == SIGCHLD || i == SIGWINCH || i == SIGURG )
                continue;
#endif
            signal( i, handler );
        }
    }

    void setupCurses() {
        if ( o_curses->boolValue() )
            Output::_output = makeCurses();
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

        o_verbose = opts.add< BoolOption >(
            "verbose", 'v', "verbose", "", "more verbose operation" );
        o_curses = opts.add< BoolOption >(
            "curses", '\0', "curses", "", "use curses-based progress monitoring" );

        o_report = common->add< BoolOption >(
            "report", 'r', "report", "", "output standardised report" );

        o_workers = common->add< IntOption >(
            "workers", 'w', "workers", "",
            "number of worker threads (default: 2)" );

        o_mem = common->add< IntOption >(
            "max-memory", '\0', "max-memory", "",
            "maximum memory to use in MB (default: 0 = unlimited)" );

        o_time = common->add< IntOption >(
            "max-time", '\0', "max-time", "",
            "maximum wall time to use in seconds (default: 0 = unlimited)" );

        o_pool = common->add< BoolOption >(
            "disable-pool", '\0', "disable-pool", "",
            "disable pooled allocation (use HOARD for all allocation)" );

        o_noCe = common->add< BoolOption >(
            "no-counterexample", 'n', "no-counterexample", "",
            "disable counterexample generation" );

        o_dispCe = common->add< BoolOption >(
            "display-counterexample", 'd', "display-counterexample", "",
            "display the counterexample after finishing" );

        o_trail = common->add< StringOption >(
            "trail", 't', "trail", "",
            "file to output trail to (default: input-file.trail)" );

        o_dummy = common->add< BoolOption >(
            "dummy", '\0', "dummy", "",
            "use a \"dummy\" benchmarking model instead of a real input" );

        o_statistics = common->add< BoolOption >(
            "statistics", 's', "statistics", "",
            "track communication and hash table load statistics" );

        o_por = common->add< BoolOption >(
            "por", 'p', "por", "",
            "enable partial order reduction" );

        o_initable = common->add< IntOption >(
            "initial-table", 'i', "initial-table", "",
            "set initial hash table size to 2^n [default = 19]" );
        o_initable ->setValue( 19 );

        cmd_metrics->add( common );
        cmd_reachability->add( common );
        cmd_owcty->add( common );
        cmd_map->add( common );
        cmd_ndfs->add( common );
        cmd_verify->add( common );
    }

    void setupLimits() {
        if ( o_time->intValue() != 0 ) {
#ifdef POSIX
            if ( o_time->intValue() < 0 ) {
                die( "FATAL: cannot have negative time limit" );
            }
            alarm( o_time->intValue() );
#else
            die( "FATAL: --max-time is not supported on win32." );
#endif
        }

        if ( o_mem->intValue() != 0 ) {
            if ( o_mem->intValue() < 0 ) {
                die( "FATAL: cannot have negative memory limit" );
            }
            if ( o_mem->intValue() < 16 ) {
                die( "FATAL: we really need at least 16M of memory" );
            }
#ifdef POSIX
            struct rlimit limit;
            limit.rlim_cur = limit.rlim_max = o_mem->intValue() * 1024 * 1024;
            if (setrlimit( RLIMIT_AS, &limit ) != 0) {
                int err = errno;
                std::cerr << "WARNING: Could not set memory limit to "
                          << o_mem->intValue() << "MB. System said: "
                          << strerror(err) << std::endl;
            }
#else
            std::cerr << "WARNING: Setting memory limit not supported "
                      << "on this platform."  << std::endl;
#endif
        }
    }


    enum { RunMetrics, RunReachability, RunNdfs, RunMap, RunOwcty, RunVerify } m_run;
    bool m_noMC;

    void parseCommandline()
    {
        std::string input;

        try {
            if ( opts.parse( argc, argv ) ) {
                if ( opts.version->boolValue() )
                    Report::about( std::cout ); // print extra version info
                exit( 0 ); // built-in command executed
            }

            if ( opts.foundCommand() == combine.cmd_combine
                 || opts.foundCommand() == compile.cmd_compile ) {
                m_noMC = true;
                return;
            }

            m_noMC = false;

            if ( !opts.hasNext() ) {
                if ( o_dummy->boolValue() )
                    dummygen = true;
                else
                    die( "FATAL: no input file specified" );
            } else {
                input = opts.next();
                if ( o_dummy->boolValue() )
                    std::cerr << "Both input and --dummy specified. Ignoring --dummy." << std::endl;
            }

        } catch( wibble::exception::BadOption &e ) {
            die( e.fullInfo() );
        }

        if ( !opts.foundCommand() )
            die( "FATAL: no command specified" );

        if ( o_workers->boolValue() )
            config.setWorkers( o_workers->intValue() );
        else
            config.setWorkers( 2 );

        config.setInput( input );
        config.setVerbose( o_verbose->boolValue() );
        config.setReport( o_report->boolValue() );
        config.setGenerateCounterexample( !o_noCe->boolValue() );
        statistics = o_statistics->boolValue();

        setupLimits();

        if ( o_trail->boolValue() ) {
            if ( o_trail->stringValue() == "" ) {
                std::string t = std::string( input, 0,
                                             input.rfind( '.' ) );
                t += ".trail";
                config.setTrailFile( t );
            } else
                config.setTrailFile( o_trail->stringValue() );
        }

        if ( o_dispCe->boolValue() ) {
            config.setCeFile( "-" );
        }

        if ( !dummygen && access( input.c_str(), R_OK ) )
            die( "FATAL: cannot open input file " + input + " for reading" );

        if ( opts.foundCommand() == cmd_verify ) {
            m_run = RunVerify;
        } else if ( opts.foundCommand() == cmd_ndfs ) {
            m_run = RunNdfs;
            if ( !o_workers->boolValue() )
                config.setWorkers( 1 );
        } else if ( opts.foundCommand() == cmd_owcty )
            m_run = RunOwcty;
        else if ( opts.foundCommand() == cmd_reachability )
            m_run = RunReachability;
        else if ( opts.foundCommand() == cmd_map )
            m_run = RunMap;
        else if ( opts.foundCommand() == cmd_metrics )
            m_run = RunMetrics;
        else
            die( "FATAL: Internal error in commandline parser." );

        config.setInitialTableSize(
            ( 1L << (o_initable->intValue()) ) / config.workers() );

        if ( config.verbose() ) {
            std::cerr << " === configured options ===" << std::endl;
            config.dump( std::cerr );
        }
    }

    template< typename Graph, typename Stats >
    Result selectAlgorithm()
    {
        if ( m_run == RunVerify ) {
            Graph temp;
            temp.read( config.input() );

            if ( temp.hasProperty() && config.workers() > 1 )
                m_run = RunOwcty;
            else {
                if ( temp.hasProperty() )
                    m_run = RunNdfs;
                else
                    m_run = RunReachability;
            }
        }

        switch ( m_run ) {
            case RunReachability:
                config.setAlgorithm( "Reachability" );
                return run< algorithm::Reachability< Graph, Stats >, Stats >();
            case RunMetrics:
                config.setAlgorithm( "Metrics" );
                return run< algorithm::Metrics< Graph, Stats >, Stats >();
            case RunOwcty:
                config.setAlgorithm( "OWCTY" );
                return run< algorithm::Owcty< Graph, Stats >, Stats >();
            case RunMap:
                config.setAlgorithm( "MAP" );
                return run< algorithm::Map< Graph, Stats >, Stats >();
            case RunNdfs:
                config.setAlgorithm( "NestedDFS" );
                return run< algorithm::NestedDFS< Graph, Stats >, Stats >();
            default:
                die( "FATAL: Internal error choosing algorithm." );
        }
    }

    template< typename Stats >
    Result selectGraph()
    {
        if ( str::endsWith( config.input(), ".dve" ) ) {
            config.setGenerator( "DVE" );
            if ( o_por->boolValue() ) {
                return selectAlgorithm< algorithm::PORGraph< generator::NDve, Stats >, Stats >();
            } else {
                return selectAlgorithm< algorithm::NonPORGraph< generator::NDve >, Stats >();
            }
        } else if ( o_por->boolValue() ) {
            die( "FATAL: Partial order reduction is not supported for this input type." );
        } else if ( str::endsWith( config.input(), ".b" ) ) {
            config.setGenerator( "NIPS" );
            return selectAlgorithm< algorithm::NonPORGraph< generator::NBymoc >, Stats >();
        } else if ( str::endsWith( config.input(), ".so" ) ) {
            config.setGenerator( "Custom" );
            return selectAlgorithm< algorithm::NonPORGraph< generator::Custom >, Stats >();
        } else if ( dummygen ) {
            config.setGenerator( "Dummy" );
            return selectAlgorithm< algorithm::NonPORGraph< generator::Dummy >, Stats >();
        } else
	    die( "FATAL: Unknown input file extension." );
    }

    template< typename Stats, typename T >
    typename T::IsDomainWorker setupParallel( Preferred, Report *r, T &t ) {
        t.domain().mpi.init();
        if ( t.domain().mpi.master() )
            setupCurses();
        Stats::global().useDomain( t.domain() );
        if ( statistics )
            Stats::global().start();
        t.domain().mpi.start();
        report->mpiInfo( t.domain().mpi );
        return wibble::Unit();
    }

    template< typename Stats, typename T >
    wibble::Unit setupParallel( NotPreferred, Report *, T &t ) {
        setupCurses();
        if ( statistics )
            Stats::global().start();
        return wibble::Unit();
    }

    template< typename Algorithm, typename Stats >
    Result run() {
        try {
            Algorithm alg( &config );
            setupParallel< Stats >( Preferred(), report, alg );
            return alg.run();
        } catch (std::exception &e) {
            die( std::string( "FATAL: " ) + e.what() );
        }
    }

    void noMC() {
        if ( opts.foundCommand() == compile.cmd_compile )
            compile.main();
        if ( opts.foundCommand() == combine.cmd_combine )
            combine.main();
    }

};

int main( int argc, const char **argv )
{
    Main m( argc, argv );
    return 0;
}
