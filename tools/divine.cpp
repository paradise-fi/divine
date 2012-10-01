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

#include <divine/utility/meta.h>
#include <divine/utility/report.h>
#include <divine/instances/instantiate.h>

#include <tools/draw.h>
#include <tools/combine.h>
#include <tools/compile.h>

#ifdef POSIX
#include <sys/resource.h>
#endif

#ifdef O_LLVM
#include <llvm/Support/Threading.h>
#endif

using namespace wibble;
using namespace commandline;

namespace divine {

Report *_report = 0;
Meta *_meta = 0;

void handler( int s ) {
    signal( s, SIG_DFL );

    sysinfo::Info i;
    Output::output().cleanup();
    if ( _report && _meta ) {
        _report->signal( s );
        _report->final( std::cout, *_meta );
    }
    raise( s );
}

struct InfoBase {
    virtual generator::PropertyType propertyType() = 0;
    virtual void read( std::string s ) = 0;
};

template< typename Setup >
struct Info : virtual algorithm::Algorithm, algorithm::AlgorithmUtils< Setup >, virtual InfoBase, Sequential
{
    void run() {
        typedef std::vector< std::pair< std::string, std::string > > Props;
        Props props;
        this->graph().getProperties( std::back_inserter( props ) );
        std::cout << "Available properties (" << props.size() << "):" << std::endl;
        for ( int i = 0; i < props.size(); ++i )
            std::cout << " " << i + 1 << ") "
                      << props[i].first << ": " << props[i].second << std::endl;
    }

    int id() { return 0; }
    virtual generator::PropertyType propertyType() {
        return this->graph().propertyType();
    }

    virtual void read( std::string s ) {
        this->graph().read( s );
    }

    Info( Meta m, bool = false ) : Algorithm( m ) {
        this->init( this );
    }
};

struct Main {
    Output *output;
    Report report;
    Meta meta;

    Engine *cmd_reachability, *cmd_owcty, *cmd_ndfs, *cmd_map, *cmd_verify,
        *cmd_metrics, *cmd_compile, *cmd_draw, *cmd_compact, *cmd_probabilistic, *cmd_info;
    OptionGroup *common, *drawing, *compact, *ce, *probabilistic, *probabilisticCommon, *reduce;
    BoolOption *o_noCe, *o_dispCe, *o_report, *o_dummy, *o_statistics;
    IntOption *o_diskfifo;
    BoolOption *o_por, *o_fair, *o_hashCompaction;
    BoolOption *o_noDeadlocks, *o_noGoals;
    BoolOption *o_curses;
    IntOption *o_workers, *o_mem, *o_time, *o_initable;
    IntOption *o_distance;
    IntOption *o_seed;
    BoolOption *o_labels, *o_traceLabels;
    StringOption *o_drawTrace, *o_output, *o_render;
    StringOption *o_trail, *o_gnuplot;
    StringOption *o_property;
    BoolOption *o_findBackEdges, *o_textFormat;
    StringOption *o_compactOutput;
    BoolOption *o_onlyQualitative, *o_disableIterativeOptimization, *o_simpleOutput; // probabilistic options

    int argc;
    const char **argv;
    commandline::StandardParserWithMandatoryCommand opts;

    Combine combine;
    Compile compile;

    Main( int _argc, const char **_argv )
        : argc( _argc ), argv( _argv ),
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

        if ( o_gnuplot->boolValue() ) {
            Statistics::global().gnuplot = true;
            Statistics::global().output = new std::ofstream( o_gnuplot->stringValue().c_str() );
        }

        run();
    }

    void run() {
        algorithm::Algorithm *a = NULL;

        if ( opts.foundCommand() == cmd_draw )
            a = selectGraph< Draw >( meta );
        if ( opts.foundCommand() == cmd_info )
            a = selectGraph< Info >( meta );

        if (!a) a = select( meta );

        if (!a)
            die( "FATAL: Internal error choosing algorithm. Built with -DSMALL?" );

        _meta = &a->meta();

        Mpi mpi; // TODO: do not construct if not needed?

        meta.execution.nodes = mpi.size();
        meta.execution.thisNode = mpi.rank();

        if ( mpi.master() ) {
            setupCurses();
            if ( o_report->boolValue() )
                _report = &report;
        }

        Statistics::global().setup( meta );
        if ( meta.output.statistics )
            Statistics::global().start();

        mpi.start();
        a->run();

        report.finished();

        if ( meta.output.statistics )
            Statistics::global().snapshot();
        Output::output().cleanup();
        if ( mpi.master() && o_report->boolValue() )
            report.final( std::cout, a->meta() );

        delete a;
        delete &Statistics::global(); // uh-oh
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
        cmd_probabilistic = opts.addEngine( "probabilistic",
                                      "<input>",
                                      "qualitative/quantitative analysis" );

        cmd_info = opts.addEngine( "info",
                                   "<input>",
                                   "show information about a model" );

        common = opts.createGroup( "Common Options" );
        drawing = opts.createGroup( "Drawing Options" );
        compact = opts.createGroup( "Compact Options" );
        probabilistic = opts.createGroup( "Probabilistic Options" );
        probabilisticCommon = opts.createGroup( "Common Options" );
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

        o_hashCompaction = cmd_reachability->add< BoolOption >(
            "hash-compaction", '\0', "hash-compaction", "",
            "reduction of memory usage, may not discover a counter-example");

        o_seed = common->add< IntOption >(
            "seed", '\0', "seed", "",
            "set seed for hashing, useful with hash-compaction" );

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

        // probabilistic options
        o_onlyQualitative = probabilistic->add< BoolOption >(
            "qualitative", 'l', "qualitative", "",
            "only qualitative analysis (default: quantitative)" );

        o_disableIterativeOptimization = probabilistic->add< BoolOption >(
            "non-iterative", 'd', "non-iterative", "",
            "disable iterative optimization for quantitative analysis" );

        o_simpleOutput = probabilistic->add< BoolOption >(
            "simple-output", 's', "simple-output", "",
            "disable verbose output" );
        probabilisticCommon->add( o_workers );
        probabilisticCommon->add( o_report );

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

        cmd_probabilistic->add( probabilisticCommon );
        cmd_probabilistic->add( probabilistic );

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


    bool m_noMC;

    void parseCommandline()
    {
        std::string input;

        try {
            if ( opts.parse( argc, argv ) ) {
                if ( opts.version->boolValue() ) {
                    sysinfo::Info i;
                    std::cout << BuildInfo();
                    std::cout << "Architecture: " << i.architecture() << std::endl;
                }
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
                    meta.input.dummygen = true;
                else
                    die( "FATAL: no input file specified" );
            } else {
                input = opts.next();
                if ( o_dummy->boolValue() )
                    std::cerr << "Both input and --dummy specified. Ignoring --dummy." << std::endl;
                while ( opts.hasNext() )
                    std::cerr << "WARNING: Extraneous argument: " << opts.next() << std::endl;
            }

        } catch( wibble::exception::BadOption &e ) {
            die( e.fullInfo() );
        }

        if ( !opts.foundCommand() )
            die( "FATAL: no command specified" );

        if ( o_workers->boolValue() )
            meta.execution.threads = o_workers->intValue();
        // else default (currently set to 2)

        meta.input.model = input;
        meta.input.propertyName = o_property->stringValue();
        meta.output.wantCe = !o_noCe->boolValue();
        meta.output.textFormat = o_textFormat->boolValue();
        meta.output.backEdges = o_findBackEdges->boolValue();
        meta.algorithm.findDeadlocks = !o_noDeadlocks->boolValue();
        meta.algorithm.findGoals = !o_noGoals->boolValue();
        meta.algorithm.hashCompaction = o_hashCompaction->boolValue();
        meta.algorithm.por = o_por->boolValue();
        meta.algorithm.hashSeed = (uint32_t) o_seed->intValue();
        meta.algorithm.fairness = o_fair->boolValue();
        meta.output.statistics = o_statistics->boolValue();

        /* No point in generating counterexamples just to discard them. */
        if ( !o_dispCe->boolValue() && !o_trail->boolValue() && !o_report->boolValue() )
            meta.output.wantCe = false;

        // probabilistic options
        meta.algorithm.onlyQualitative = o_onlyQualitative->boolValue();
        meta.algorithm.iterativeOptimization = !o_disableIterativeOptimization->boolValue();
        meta.output.quiet = o_simpleOutput->boolValue();

        meta.algorithm.maxDistance = o_distance->intValue();
        meta.output.filterProgram = o_render->stringValue();
        meta.algorithm.labels = o_labels->boolValue();
        meta.algorithm.traceLabels = o_traceLabels->boolValue();
        meta.input.trace = o_drawTrace->stringValue();

        setupLimits();

        if ( o_trail->boolValue() ) {
            if ( o_trail->stringValue() == "" ) {
                std::string t = std::string( input, 0,
                                             input.rfind( '.' ) );
                t += ".trail";
                meta.output.trailFile = t;
            } else
                meta.output.trailFile = o_trail->stringValue();
        }

        meta.output.file = o_output->stringValue();

        if ( o_dispCe->boolValue() ) {
            meta.output.ceFile = "-";
        }

        if ( !meta.input.dummygen && access( input.c_str(), R_OK ) )
            die( "FATAL: cannot open input file " + input + " for reading" );

        if ( opts.foundCommand() == cmd_draw ) {
            meta.execution.threads = 1; // never runs in parallel
            meta.algorithm.algorithm = meta::Algorithm::Draw;
        } else if ( opts.foundCommand() == cmd_info ) {
            meta.algorithm.algorithm = meta::Algorithm::Info;
        } else if ( opts.foundCommand() == cmd_ndfs ) {
            meta.algorithm.algorithm = meta::Algorithm::Ndfs;
            if ( !o_workers->boolValue() )
                meta.execution.threads = 1;
        } else if ( opts.foundCommand() == cmd_owcty )
            meta.algorithm.algorithm = meta::Algorithm::Owcty;
        else if ( opts.foundCommand() == cmd_reachability ) {
            if ( !meta.algorithm.findGoals && !meta.algorithm.findDeadlocks )
                std::cerr << "WARNING: Both deadlock and goal detection is disabled."
                          << "Only state count " << std::endl << "statistics will be tracked "
                          << "(\"divine metrics\" would have been more efficient)." << std::endl;
            meta.algorithm.algorithm = meta::Algorithm::Reachability;
        } else if ( opts.foundCommand() == cmd_map )
            meta.algorithm.algorithm = meta::Algorithm::Map;
        else if ( opts.foundCommand() == cmd_metrics )
            meta.algorithm.algorithm = meta::Algorithm::Metrics;
        else if ( opts.foundCommand() == cmd_compact ) {

            if ( o_compactOutput->stringValue() == "" ) {
                meta.output.file = str::basename( input + ".compact" );
            } else if ( o_compactOutput->stringValue() == "-" ) {
                meta.output.file = "";
            } else {
                meta.output.file = o_compactOutput->stringValue();
            }

            meta.algorithm.algorithm = meta::Algorithm::Compact;
        } else if ( opts.foundCommand() == cmd_probabilistic ) {
            meta.algorithm.algorithm = meta::Algorithm::Probabilistic;
        } else if ( opts.foundCommand() == cmd_verify ) {
            InfoBase *ib = dynamic_cast< InfoBase * >( selectGraph< Info >( meta ) );
            assert( ib );

            ib->read( meta.input.model );
            if ( ib->propertyType() == generator::AC_None )
                meta.algorithm.algorithm = meta::Algorithm::Reachability;
            else {
                if ( meta.execution.threads > 1 || meta.execution.nodes > 1 )
                    meta.algorithm.algorithm = meta::Algorithm::Owcty;
                else
                    meta.algorithm.algorithm = meta::Algorithm::Ndfs;
            }

            delete ib;
        }
        else
            die( "FATAL: Internal error in commandline parser." );

        meta.execution.initialTable = 1L << (o_initable->intValue());
        if (opts.foundCommand() != cmd_ndfs) // ndfs uses shared table
            meta.execution.initialTable /= meta.execution.threads;
    }

#if 0
    template< typename Graph, typename Stats >
    void selectAlgorithm()
    {
        if ( m_run == RunVerify ) {
            Graph temp;
            temp.read( meta.input.model );

            if ( temp.propertyType() != generator::AC_None && meta.execution.threads > 1 )
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
                meta.algorithm.name = "Draw";
                return run< Draw< Graph >, Stats >();
            case RunInfo:
                meta.algorithm.name = "Info";
                return run< Info< Graph >, Stats >();
            default:
                die( "FATAL: Internal error choosing algorithm. Built with -DSMALL?" );
        }
    }

    void runProbabilistic() {
        meta.algorithm.name = "Probabilistic";
        try {
            assert( meta.input.model.substr( meta.input.model.rfind( '.' ) ) == ".compact" );

            algorithm::Probabilistic< algorithm::NonPORGraph< generator::Compact > > alg( &meta );
            alg.domain().mpi.init();
            alg.init( &alg.domain() );
            alg.domain().mpi.start();
            report.mpiInfo( alg.domain().mpi );

            alg.run();
        } catch ( std::exception &e ) {
            die( std::string( "FATAL: " ) + e.what() );
        } catch ( const char* s ) {
            die( std::string( "FATAL: " ) + s );
        }
    }

    template< typename Stats, typename T >
    typename T::IsDomainWorker setupParallel( Preferred, T &t ) {
        t.domain().mpi.init();
        t.init( &t.domain(), o_diskfifo->intValue() );
        if ( t.domain().mpi.master() )
            setupCurses();
        Stats::global().useDomain( t.domain() );
        if ( statistics )
            Stats::global().start();
        t.domain().mpi.start();
        report.mpiInfo( t.domain().mpi );
        return wibble::Unit();
    }

    template< typename Stats, typename T >
    wibble::Unit setupParallel( NotPreferred, T &t ) {
        t.init();
        setupCurses();
        if ( statistics )
            Stats::global().start();
        return wibble::Unit();
    }

    template< typename Algorithm, typename Stats >
    void run() {
        try {
            Algorithm alg( &meta );
            setupParallel< Stats >( Preferred(), alg );
            alg.run();
        } catch (std::exception &e) {
            die( std::string( "FATAL: " ) + e.what() );
        }
    }
#endif

    void noMC() {
        if ( opts.foundCommand() == compile.cmd_compile )
            compile.main();
        if ( opts.foundCommand() == combine.cmd_combine )
            combine.main();
    }

};

}

int main( int argc, const char **argv )
{
    divine::Main m( argc, argv );
    return 0;
}
