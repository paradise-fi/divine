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
#include <divine/generator/common.h>
#include <divine/generator/dummy.h>
#include <divine/generator/legacy.h>
#include <divine/generator/custom.h>
#include <divine/generator/compact.h>
#include <divine/generator/coin.h>
#include <divine/generator/llvm.h>
#include <divine/generator/dve.h>

// The algorithms.
#include <divine/algorithm/reachability.h>
#include <divine/algorithm/owcty.h>
#include <divine/algorithm/metrics.h>
#include <divine/algorithm/map.h>
#include <divine/algorithm/nested-dfs.h>
#include <divine/algorithm/compact.h>

#include <divine/porcp.h>
#include <divine/fairgraph.h>

#include <divine/report.h>

#include <tools/draw.h>
#include <tools/combine.h>
#include <tools/compile.h>

#ifdef POSIX
#include <sys/resource.h>
#endif

#ifdef O_LLVM
#include <llvm/Support/Threading.h>
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

template< typename G >
struct Info : virtual algorithm::Algorithm, algorithm::AlgorithmUtils< G >
{
    G g;
    Result run() {
        typedef std::vector< std::pair< std::string, std::string > > Props;
        Props props;
        g.getProperties( std::back_inserter( props ) );
        std::cout << "Available properties:" << std::endl;
        for ( int i = 0; i < props.size(); ++i )
            std::cout << " " << i + 1 << ") "
                      << props[i].first << ": " << props[i].second << std::endl;
        return Result();
    }

    Info( Config *c ) : Algorithm( c ) {
        this->initPeer( &g, 0 ); // XXX icky
    }
};

struct Main {
    Config config;
    DrawConfig drawConfig;
    Output *output;
    Report *report;

    Engine *cmd_reachability, *cmd_owcty, *cmd_ndfs, *cmd_map, *cmd_verify,
        *cmd_metrics, *cmd_compile, *cmd_draw, *cmd_compact, *cmd_info;
    OptionGroup *common, *drawing, *compact, *ce, *reduce;
    BoolOption *o_pool, *o_noCe, *o_dispCe, *o_report, *o_dummy, *o_statistics;
    IntOption *o_diskfifo;
    BoolOption *o_por, *o_fair;
    BoolOption *o_noDeadlocks, *o_noGoals;
    BoolOption *o_curses;
    IntOption *o_workers, *o_mem, *o_time, *o_initable;
    IntOption *o_distance;
    BoolOption *o_labels, *o_traceLabels;
    StringOption *o_drawTrace, *o_output, *o_render;
    StringOption *o_trail, *o_gnuplot;
    StringOption *o_property;
    BoolOption *o_findBackEdges, *o_textFormat;
    StringOption *o_compactOutput;

    bool dummygen;
    bool statistics;

    int argc;
    const char **argv;
    commandline::StandardParserWithMandatoryCommand opts;

    Combine combine;
    Compile compile;

    Main( int _argc, const char **_argv )
        : report( 0 ),
          dummygen( false ), statistics( false ), argc( _argc ), argv( _argv ),
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
        if ( o_report->boolValue() )
            ::report = &rep;
        Result res;

        if ( o_gnuplot->boolValue() ) {
            Statistics::global().gnuplot = true;
            Statistics::global().output = new std::ofstream( o_gnuplot->stringValue().c_str() );
        }

#ifdef O_PERFORMANCE
        if ( statistics )
            res = selectGraph< Statistics >();
        else
            res = selectGraph< NoStatistics >();
#else
        res = selectGraph< Statistics >();
#endif
        rep.finished( res );
        if ( statistics )
            Statistics::global().snapshot();
        Output::output().cleanup();
        if ( o_report->boolValue() )
            rep.final( std::cout );
    }

    static void die( std::string bla ) __attribute__((noreturn))
    {
        std::cerr << bla << std::endl;
        exit( 1 );
    }

    void FIXME( std::string x ) __attribute__((noreturn)) { die( x ); }

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

        cmd_draw = opts.addEngine( "draw",
                                   "<input>",
                                   "draw (part of) the state space" );
        cmd_compact = opts.addEngine( "compact",
                                      "<input>",
                                      "compact state space representation" ); 

        cmd_info = opts.addEngine( "info",
                                   "<input>",
                                   "show information about a model" );

        common = opts.createGroup( "Common Options" );
        drawing = opts.createGroup( "Drawing Options" );
        compact = opts.createGroup( "Compact Options" );
        reduce = opts.createGroup( "Reduction Options" );
        ce = opts.createGroup( "Counterexample Options" );

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

        o_dummy = common->add< BoolOption >(
            "dummy", '\0', "dummy", "",
            "use a \"dummy\" benchmarking model instead of a real input" );

        o_statistics = common->add< BoolOption >(
            "statistics", 's', "statistics", "",
            "track communication and hash table load statistics" );
        o_gnuplot= common->add< StringOption >(
            "gnuplot-statistics", '\0', "gnuplot-statistics", "",
            "output statistics in a gnuplot-friendly format" );

        o_por = reduce->add< BoolOption >(
            "por", '\0', "por", "",
            "enable partial order reduction" );

        o_fair = reduce->add< BoolOption >(
            "fairness", 'f', "fair", "",
            "consider only weakly fair executions" );

        o_noGoals = cmd_reachability->add< BoolOption >(
            "ignore-goals", '\0', "ignore-goals", "",
            "ignore goal states" );

        o_noDeadlocks = cmd_reachability->add< BoolOption >(
            "ignore-deadlocks", '\0', "ignore-deadlocks", "",
            "ignore deadlock states" );

        o_initable = common->add< IntOption >(
            "initial-table", 'i', "initial-table", "",
            "set initial hash table size to 2^n [default = 19]" );
        o_initable ->setValue( 19 );

        o_diskfifo = common->add< IntOption >(
            "disk-fifo", '\0', "disk-fifo", "",
            "save long queues on disk to reduce memory usage" );
        o_property = common->add< StringOption >(
            "property", 'p', "property", "",
            "select a (non-default) property" );

        // counterexample options
        o_noCe = ce->add< BoolOption >(
            "no-counterexample", 'n', "no-counterexample", "",
            "disable counterexample generation" );

        o_dispCe = ce->add< BoolOption >(
            "display-counterexample", 'd', "display-counterexample", "",
            "display the counterexample after finishing" );

        o_trail = ce->add< StringOption >(
            "trail", 't', "trail", "",
            "file to output trail to (default: input-file.trail)" );

        // drawing options
        o_distance = drawing->add< IntOption >(
            "distance", '\0', "distance", "",
            "set maximum BFS distance from initial state [default = 32]" );
        o_distance ->setValue( 32 );

        o_drawTrace = drawing->add< StringOption >(
            "draw-trace", '\0', "draw-trace", "",
            "draw and highlight a particular trace in the output" );
        o_output = drawing->add< StringOption >(
            "output", 'o', "output", "",
            "the output file name (display to X11 if not specified)" );
        o_render = drawing->add< StringOption >(
            "render", 'r', "render", "",
            "command to render the graphviz description [default=dot -Tx11]" );
        o_labels = drawing->add< BoolOption >(
            "labels", 'l', "labels", "", "draw state labels" );
        o_traceLabels = drawing->add< BoolOption >(
            "trace-labels", '\0', "trace-labels", "", "draw state labels, in trace only" );

        // compact options
        o_findBackEdges = compact->add< BoolOption >(
            "find-back-transitions", 'b', "find-back-transitions", "",
            "find also backward transitions" );
        o_textFormat = compact->add< BoolOption >(
            "text-format", 't', "text-format", "",
            "output compact state space in plaintext format" );
        o_compactOutput = compact->add< StringOption >(
            "compact-output", 'm', "compact-output", "",
            "where to output the compacted state space (default: ./input-file.compact, -: stdout)" );

        cmd_metrics->add( common );
        cmd_metrics->add( reduce );

        cmd_reachability->add( common );
        cmd_reachability->add( ce );
        cmd_reachability->add( reduce );

        cmd_owcty->add( common );
        cmd_owcty->add( ce );
        cmd_owcty->add( reduce );

        cmd_map->add( common );
        cmd_map->add( ce );
        cmd_map->add( reduce );

        cmd_ndfs->add( common );
        cmd_ndfs->add( ce );
        cmd_ndfs->add( reduce );

        cmd_verify->add( common );
        cmd_verify->add( ce );
        cmd_verify->add( reduce );

        cmd_compact->add( common );
        cmd_compact->add( compact );
        cmd_compact->add( reduce );

        cmd_draw->add( drawing );
        cmd_draw->add( reduce );
        cmd_draw->add( o_property );
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


    enum RunAlgorithm { RunMetrics, RunReachability, RunNdfs, RunMap, RunOwcty, RunVerify,
                        RunDraw, RunInfo, RunCompact } m_run;
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
            config.workers = o_workers->intValue();
        // else default (currently set to 2)

        config.input = input;
        config.property = o_property->stringValue();
        config.wantCe = !o_noCe->boolValue();
        config.textFormat = o_textFormat->boolValue();
        config.findBackEdges = o_findBackEdges->boolValue();
        config.findDeadlocks = !o_noDeadlocks->boolValue();
        config.findGoals = !o_noGoals->boolValue();
        statistics = o_statistics->boolValue();

        /* No point in generating counterexamples just to discard them. */
        if ( !o_dispCe->boolValue() && !o_trail->boolValue() && !o_report->boolValue() )
            config.wantCe = false;

        drawConfig.maxDistance = o_distance->intValue();
        drawConfig.output = o_output->stringValue();
        drawConfig.render= o_render->stringValue();
        drawConfig.labels = o_labels->boolValue();
        drawConfig.traceLabels = o_traceLabels->boolValue();
        drawConfig.drawTrace = o_drawTrace->stringValue();

        setupLimits();

        if ( o_trail->boolValue() ) {
            if ( o_trail->stringValue() == "" ) {
                std::string t = std::string( input, 0,
                                             input.rfind( '.' ) );
                t += ".trail";
                config.trailFile = t;
            } else
                config.trailFile = o_trail->stringValue();
        }

        if ( o_compactOutput->stringValue() == "" ) {
            std::string t = std::string( input, 0, input.rfind( '.' ) );
            t += ".compact";
            config.compactFile = str::basename( t );
        } else if ( o_compactOutput->stringValue() == "-" ) {
            config.compactFile = "";
        } else {
            config.compactFile = o_compactOutput->stringValue();
        }

        if ( o_dispCe->boolValue() ) {
            config.ceFile = "-";
        }

        if ( !dummygen && access( input.c_str(), R_OK ) )
            die( "FATAL: cannot open input file " + input + " for reading" );

        if ( opts.foundCommand() == cmd_draw ) {
            config.workers = 1; // never runs in parallel
            m_run = RunDraw;
        } else if ( opts.foundCommand() == cmd_info ) {
            m_run = RunInfo;
        } else if ( opts.foundCommand() == cmd_verify ) {
            m_run = RunVerify;
        } else if ( opts.foundCommand() == cmd_ndfs ) {
            m_run = RunNdfs;
            if ( !o_workers->boolValue() )
                config.workers = 1;
        } else if ( opts.foundCommand() == cmd_owcty )
            m_run = RunOwcty;
        else if ( opts.foundCommand() == cmd_reachability ) {
            if ( !config.findGoals && !config.findDeadlocks )
                std::cerr << "WARNING: Both deadlock and goal detection is disabled."
                          << "Only state count " << std::endl << "statistics will be tracked "
                          << "(\"divine metrics\" would have been more efficient)." << std::endl;
            m_run = RunReachability;
        } else if ( opts.foundCommand() == cmd_map )
            m_run = RunMap;
        else if ( opts.foundCommand() == cmd_metrics )
            m_run = RunMetrics;
        else if ( opts.foundCommand() == cmd_compact )
            m_run = RunCompact;
        else
            die( "FATAL: Internal error in commandline parser." );

        config.initialTable = 1L << (o_initable->intValue());
        if (opts.foundCommand() != cmd_ndfs) // ndfs uses shared table
            config.initialTable /= config.workers;
    }

    template< typename Graph, typename Stats >
    Result selectAlgorithm()
    {
        if ( m_run == RunVerify ) {
            Graph temp;
            temp.read( config.input );

            if ( temp.propertyType() != generator::AC_None && config.workers > 1 )
                m_run = RunOwcty;
            else {
                if ( temp.propertyType() != generator::AC_None )
                    m_run = RunNdfs;
                else
                    m_run = RunReachability;
            }
        }

        switch ( m_run ) {
            case RunDraw:
                report->algorithm = "Draw";
                return run< Draw< Graph >, Stats >();
            case RunInfo:
                report->algorithm = "Info";
                return run< Info< Graph >, Stats >();
            case RunReachability:
                report->algorithm = "Reachability";
                return run< algorithm::Reachability< Graph, Stats >, Stats >();
            case RunMetrics:
                report->algorithm = "Metrics";
                return run< algorithm::Metrics< Graph, Stats >, Stats >();
            case RunOwcty:
                report->algorithm = "OWCTY";
                return run< algorithm::Owcty< Graph, Stats >, Stats >();
#ifndef O_SMALL
            case RunMap:
                report->algorithm = "MAP";
                return run< algorithm::Map< Graph, Stats >, Stats >();
#endif
            case RunNdfs:
                report->algorithm = "Nested DFS";
                return run< algorithm::NestedDFS< Graph, Stats >, Stats >();
#ifndef O_SMALL
            case RunCompact:
                report->algorithm = "Compact";
                return run< algorithm::Compact< Graph, Stats >, Stats >();
#endif
            default:
                die( "FATAL: Internal error choosing algorithm. Built with -DSMALL?" );
        }
    }

    template< typename Stats >
    Result selectGraph()
    {
        if ( str::endsWith( config.input, ".dve" ) ) {
            report->generator = "DVE";
#ifndef O_SMALL
            if ( o_fair->boolValue() ) {
                if ( o_por->boolValue() )
                    std::cerr << "Fairness with POR is not supported, disabling POR" << std::endl;
                report->reductions.push_back( "FAIRNESS" );
                return selectAlgorithm< algorithm::FairGraph< generator::LegacyDve >, Stats >();
            }
            if ( o_por->boolValue() ) {
                report->reductions.push_back( "POR" );
                return selectAlgorithm< algorithm::PORGraph< generator::LegacyDve, Stats >, Stats >();
            } else
#endif
            {
#ifdef O_DVE
                return selectAlgorithm< algorithm::NonPORGraph< generator::Dve >, Stats >();
#else
                return selectAlgorithm< algorithm::NonPORGraph< generator::LegacyDve >, Stats >();
#endif
            }
        } else if ( str::endsWith( config.input, ".probdve" ) ) {
            report->generator = "ProbDVE";
            return selectAlgorithm< algorithm::NonPORGraph< generator::NProbDve >, Stats >();
        } else if ( str::endsWith( config.input, ".compact" ) ) {
            report->generator = "Compact";
            return selectAlgorithm< algorithm::NonPORGraph< generator::Compact >, Stats >();
#ifndef O_SMALL
        } else if ( str::endsWith( config.input, ".coin" ) ) {
	    report->generator = "CoIn";
            if ( o_por->boolValue() ) {
                report->reductions.push_back( "POR" );
                return selectAlgorithm< algorithm::PORGraph< generator::Coin, Stats >, Stats >();
            } else {
                return selectAlgorithm< algorithm::NonPORGraph< generator::Coin >, Stats >();
            }
#endif
        } else if ( o_por->boolValue() ) {
            die( "FATAL: Partial order reduction is not supported for this input type." );
        } else if ( str::endsWith( config.input, ".bc" ) ) {
            report->generator = "LLVM";
#ifdef O_LLVM
            if ( config.workers > 1 && !::llvm::llvm_start_multithreaded() )
                die( "FATAL: This binary is linked to single-threaded LLVM.\n"
                     "Multi-threaded LLVM is required for parallel algorithms." );
            return selectAlgorithm< algorithm::NonPORGraph< generator::LLVM >, Stats >();
#else
	    die( "FATAL: This binary was built without LLVM support." );
#endif

#ifndef O_SMALL
        } else if ( str::endsWith( config.input, ".b" ) ) {
            report->generator = "NIPS";
            return selectAlgorithm< algorithm::NonPORGraph< generator::LegacyBymoc >, Stats >();
#endif
        } else if ( str::endsWith( config.input, ".so" ) ) {
            report->generator = "Custom";
            return selectAlgorithm< algorithm::NonPORGraph< generator::Custom >, Stats >();
#ifndef O_SMALL
        } else if ( dummygen ) {
            report->generator = "Dummy";
            return selectAlgorithm< algorithm::NonPORGraph< generator::Dummy >, Stats >();
#endif
        } else
	    die( "FATAL: Unknown input file extension." );

        die( "FATAL: Internal error choosing generator." );
    }

    template< typename Stats, typename T >
    typename T::IsDomainWorker setupParallel( Preferred, Report *r, T &t ) {
        t.domain().mpi.init();
        t.init( &t.domain(), o_diskfifo->intValue() );
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
        t.init();
        setupCurses();
        if ( statistics )
            Stats::global().start();
        return wibble::Unit();
    }

    template< typename C >
    C *configure() { assert_die(); }

    template< typename Algorithm, typename Stats >
    Result run() {
        try {
            Algorithm alg( configure< typename Algorithm::Config >() );
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

template<> Config *Main::configure() { return &config; }
template<> DrawConfig *Main::configure() {
    drawConfig.super = &config;
    return &drawConfig;
}


int main( int argc, const char **argv )
{
    Main m( argc, argv );
    return 0;
}
