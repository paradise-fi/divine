// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>
#include <wibble/test.h> // for assert
#include <queue>
#include <iostream>
#include <sstream>
#include <memory>

#include <wibble/sys/thread.h>
#include <wibble/sys/mutex.h>
#include <wibble/commandline/parser.h>
#include <wibble/string.h>
#include <wibble/sfinae.h>
#include <wibble/sys/fs.h>

#include <divine/utility/meta.h>
#include <divine/utility/report.h>
#include <divine/instances/select.h>

#include <tools/draw.h>
#include <tools/combine.h>
#include <tools/compile.h>
#include <tools/info.h>

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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    signal( s, SIG_DFL );
#pragma GCC diagnostic pop

    sysinfo::Info i;
    Output::output().cleanup();
    if ( _report && _meta ) {
        _report->signal( s );
        _report->final( std::cout, *_meta );
    }
    raise( s );
}

struct Main {
    Output *output;
    Report report;
    Meta meta;

    Engine *cmd_verify, *cmd_metrics, *cmd_compile, *cmd_draw, *cmd_info,
           *cmd_simulate, *cmd_compact;
    OptionGroup *common, *drawing, *input, *reduce, *compression, *definitions,
                *ce, *compactOutput;
    BoolOption *o_noCe, *o_dispCe, *o_report, *o_dummy, *o_statistics;
    BoolOption *o_diskFifo;
    BoolOption *o_fair, *o_hashCompaction, *o_shared;
    StringOption *o_reduce;
    StringOption *o_compression;
    VectorOption< String > *o_definitions;
    BoolOption *o_noreduce;
    BoolOption *o_curses;
    IntOption *o_workers, *o_mem, *o_time, *o_initable;
    IntOption *o_distance;
    IntOption *o_seed;
    BoolOption *o_labels, *o_traceLabels, *o_bfsLayout;
    StringOption *o_drawTrace, *o_drawOutput, *o_render;
    StringOption *o_gnuplot;
    StringOption *o_property;
    StringOption *o_inputTrace;
    StringOption *o_demangle;
    StringOption *o_outputFile;
    BoolOption *o_noSaveStates;

    BoolOption *o_ndfs, *o_map, *o_owcty, *o_reachability;
    BoolOption *o_mpi, *o_probabilistic;

    int argc;
    const char **argv;
    commandline::StandardParserWithMandatoryCommand opts;

    Combine combine;
    Compile compile;

    std::unique_ptr< Mpi > mpi;

    void mpiFillMeta( Meta &meta ) {
        // needs to be set up before parseCommandline
        meta.execution.nodes = mpi->size();
        meta.execution.thisNode = mpi->rank();
    }

    Main( int _argc, const char **_argv )
        : argc( _argc ), argv( _argv ),
          opts( "DiVinE", versionString(), 1, "DiVinE Team <divine@fi.muni.cz>" ),
          combine( opts, argc, argv ),
          compile( opts )
    {
        {
            std::ostringstream execCommStr;
            for( int i = 0; i < argc; ++i )
                execCommStr << argv[i] << " ";
            report.execCommand = execCommStr.str();
        }

        setupSignals();
        setupCommandline();
        parseCommandline();

        if ( opts.foundCommand() == compile.cmd_compile ) {
            compile.main();
            return;
        }

        if ( opts.foundCommand() == combine.cmd_combine ) {
            combine.main();
            return;
        }

        if ( o_gnuplot->boolValue() ) {
            TrackStatistics::global().gnuplot = NoStatistics::global().gnuplot
                = true;
            TrackStatistics::global().output = NoStatistics::global().output
                = new std::ofstream( o_gnuplot->stringValue().c_str() );
        }

        run();
    }

    void run() {
        if ( !mpi )
            mpi.reset( new Mpi( o_mpi->boolValue() ) );

        mpiFillMeta( meta );

        auto a = select( meta );

        if (!a)
            die( "FATAL: Internal error choosing algorithm. Built with -DSMALL?" );

        _meta = &a->meta();

        assert_eq( a->meta().execution.nodes, mpi->size() );
        assert_eq( a->meta().execution.thisNode, mpi->rank() );

        if ( mpi->master() ) {
            setupOutput();
            if ( o_report->boolValue() )
                _report = &report;
        }

        TrackStatistics::global().setup( a->meta() );
        if ( meta.output.statistics )
            TrackStatistics::global().start();


        mpi->start();

        if ( mpi->master() && opts.foundCommand() != cmd_info ) {
            auto a = meta.algorithm.name;
            std::cerr << " input: " << meta.input.model << std::endl
                      << " property " << meta.input.propertyName
                      << ": " << meta.input.property << std::endl
                      << " " << std::string( (44 - a.size()) / 2, '-' )
                      << " " << a << " " << std::string( (44 - a.size()) / 2 , '-' )
                      << std::endl;
        }

        a->run();

        report.finished();

        if ( meta.output.statistics )
            TrackStatistics::global().snapshot();
        Output::output().cleanup();
        if ( mpi->master() && o_report->boolValue() )
            report.final( std::cout, a->meta() );
    }

    static void die( std::string bla ) __attribute__((noreturn))
    {
        std::cerr << bla << std::endl;
        exit( 1 );
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

    void setupOutput() {
        if ( o_curses->boolValue() )
            Output::_output.reset( makeCurses() );
        else
            Output::_output.reset( makeStdIO( std::cerr ) );
    }

    void setupCommandline()
    {
        opts.usage = "<command> [command-specific options] <input file>";
        opts.description = "A Parallel Explicit-State LTL Model Checker";

        cmd_metrics = opts.addEngine( "metrics",
                                      "<input>",
                                      "state space metrics");
        cmd_verify = opts.addEngine( "verify",
                                     "<input>",
                                     "verification" );
        cmd_draw = opts.addEngine( "draw",
                                   "<input>",
                                   "draw (part of) the state space" );
        cmd_info = opts.addEngine( "info",
                                   "<input>",
                                   "show information about a model" );
        cmd_simulate = opts.addEngine( "simulate",
                                       "<input>",
                                       "explore a state-space interactively" );
        cmd_compact = opts.addEngine( "compact",
                                      "<input>",
                                      "convert state-space to compact explicit representation" );

        common = opts.createGroup( "Common Options" );
        drawing = opts.createGroup( "Drawing Options" );
        input = opts.createGroup( "Input Options" );
        reduce = opts.createGroup( "Reduction Options" );
        compression = opts.createGroup( "Compression Options" );
        definitions = opts.createGroup( "Definitions" );
        ce = opts.createGroup( "Counterexample Options" );
        compactOutput = opts.createGroup( "Output option" );

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

        o_statistics = common->add< BoolOption >(
            "statistics", 's', "statistics", "",
            "track communication and hash table load statistics" );
        o_gnuplot= common->add< StringOption >(
            "gnuplot-statistics", '\0', "gnuplot-statistics", "",
            "output statistics in a gnuplot-friendly format" );

        o_reduce = reduce->add< StringOption >(
            "reduce", '\0', "reduce", "",
            "configure reductions (input language dependent) [default = tau+,taustores,heap,por,LU]" );
        o_noreduce = reduce->add< BoolOption >(
            "no-reduce", '\0', "no-reduce", "",
            "disable all state space reductions" );
        o_fair = reduce->add< BoolOption >(
            "fairness", 'f', "fair", "",
            "consider only weakly fair executions" );

        o_compression = compression->add< StringOption >(
                "compression", '\0', "compression", "",
                "configure state compression [default = none], available: none, tree" );

        o_initable = common->add< IntOption >(
            "initial-table", 'i', "initial-table", "",
            "set initial hash table size to 2^n [default = 19]" );
        o_initable ->setValue( 19 );

        o_diskFifo = common->add< BoolOption >(
            "disk-fifo", '\0', "disk-fifo", "",
            "save long queues to disk to reduce memory usage" );

        o_hashCompaction = common->add< BoolOption >(
            "hash-compaction", '\0', "hash-compaction", "",
            "reduction of memory usage, may not discover a counter-example");

        o_shared = common->add< BoolOption >(
            "shared-memory", '\0', "shared", "",
            "enable shared-memory hashtables & queues (EXPERIMENTAL)");

        o_seed = common->add< IntOption >(
            "seed", '\0', "seed", "",
            "set seed for hashing, useful with hash-compaction" );

        o_property = common->add< StringOption >(
            "property", 'p', "property", "",
            "select a property [default=deadlock]" );

        o_mpi = common->add< BoolOption >(
                "mpi", 0, "mpi", "",
                "Force use of MPI (in case it is not detected properly)" );

        o_demangle = common->add< StringOption >(
                "demangle", 0, "demangle", "",
                "Demagle style of symbols (only for LLVM verification) [default=none, available=node,cpp]" );

        // definitions
        o_definitions = definitions->add< VectorOption< String > >(
            "definition", 'D', "definition", "",
            "add definition for generator (can be specified multiple times)" );

        // counterexample options
        o_noCe = ce->add< BoolOption >(
            "no-counterexample", 'n', "no-counterexample", "",
            "disable counterexample generation" );

        o_dispCe = ce->add< BoolOption >(
            "display-counterexample", 'd', "display-counterexample", "",
            "display the counterexample after finishing" );

        // input options
        o_dummy = input->add< BoolOption >(
            "dummy", '\0', "dummy", "",
            "use a \"dummy\" benchmarking model instead of real input" );
        o_probabilistic = input->add< BoolOption >(
            "probabilistic", '\0', "probabilistic", "",
            "enable probabilistic extensions (where available)" );

        // drawing options
        o_distance = drawing->add< IntOption >(
            "distance", '\0', "distance", "",
            "set maximum BFS distance from initial state [default = 32]" );
        o_distance ->setValue( 32 );
        o_drawTrace = drawing->add< StringOption >(
            "trace", '\0', "trace", "",
            "draw and highlight a particular trace in the output" );
        o_drawOutput = drawing->add< StringOption >(
            "output", 'o', "output", "",
            "the output file name (display to X11 if not specified)" );
        o_render = drawing->add< StringOption >(
            "render", 'r', "render", "",
            "command to render the graphviz description [default=dot -Tx11]" );
        o_labels = drawing->add< BoolOption >(
            "labels", 'l', "labels", "", "draw state labels" );
        o_traceLabels = drawing->add< BoolOption >(
            "trace-labels", '\0', "trace-labels", "", "draw state labels, in trace only" );
        o_bfsLayout = drawing->add< BoolOption >(
            "bfs-layout", '\0', "bfs-layout", "", "ask dot to lay out BFS layers in rows" );

        // verify options
        o_ndfs = cmd_verify->add< BoolOption >(
            "nested-dfs", 0, "nested-dfs", "", "force use of Nested DFS" );
        o_map = cmd_verify->add< BoolOption >(
            "map", 0, "map", "", "force use of MAP" );
        o_owcty = cmd_verify->add< BoolOption >(
            "owcty", 0, "owcty", "", "force use of OWCTY" );
        o_reachability = cmd_verify->add< BoolOption >(
            "reachability", 0, "reachability", "", "force reachability" );

        // simulate options
        o_inputTrace = cmd_simulate->add< StringOption >(
            "trace", '\0', "trace", "",
            "generate states of a numeric trace and exit" );

        o_outputFile = compactOutput->add< StringOption >(
                "output", 'o', "output", "",
                "the output file name (if not specified <current-dir>/<model-name>.dcess is used)" );
        o_noSaveStates = compactOutput->add< BoolOption >(
                "no-save-states", 0, "no-save-states", "",
                "do not save states in DCESS file, only save transitions." );

        cmd_metrics->add( common );
        cmd_metrics->add( reduce );
        cmd_metrics->add( compression );
	cmd_metrics->add( input );
        cmd_metrics->add( definitions );

        cmd_verify->add( common );
        cmd_verify->add( ce );
        cmd_verify->add( reduce );
        cmd_verify->add( compression );
	cmd_verify->add( input );
        cmd_verify->add( definitions );

        cmd_simulate->add( common );
        cmd_simulate->add( reduce );
        cmd_simulate->add( compression );
        cmd_simulate->add( definitions );

        cmd_compact->add( common );
        cmd_compact->add( reduce );
        cmd_compact->add( compression );
        cmd_compact->add( definitions );
        cmd_compact->add( input );
        cmd_compact->add( compactOutput );

        cmd_draw->add( drawing );
        cmd_draw->add( reduce );
        cmd_draw->add( o_property );
        cmd_draw->add( compression );
	cmd_draw->add( input );
        cmd_draw->add( definitions );
    }

    void setupLimits() {
        int64_t time, memory;
        time = o_time->intValue();
        memory = o_mem->intValue();

        if ( time < 0 )
            die( "FATAL: cannot have negative time limit" );

        if ( memory < 0 )
            die( "FATAL: cannot have negative memory limit" );

        if ( memory && memory < 256 )
            die( "FATAL: we really need at least 256M of memory" );

        if ( time || memory ) {
            auto rg = new sysinfo::ResourceGuard();
            rg->memory = memory * 1024;
            rg->time = time;
            rg->start();
        }
    }


    graph::ReductionSet parseReductions( std::string s )
    {
        wibble::str::Split splitter( ",", s );
        graph::ReductionSet r;
        std::transform( splitter.begin(), splitter.end(), std::inserter( r, r.begin() ),
                        [&]( std::string s ) {
                            if ( s == "tau" ) return graph::R_Tau;
                            if ( s == "tau+" ) return graph::R_TauPlus;
                            if ( s == "por" ) return graph::R_POR;
                            if ( s == "taustores" ) return graph::R_TauStores;
                            if ( s == "heap" ) return graph::R_Heap;
                            if ( s == "LU" ) return graph::R_LU;
                            throw wibble::exception::OutOfRange(
                                "reduction", "'" + s + "' is not a known reduction type;\n"
                                "tau, tau+, por, taustores, heap and LU are allowed" );
                        } );
        return r;
    }

    meta::Algorithm::CompressionType parseCompression( std::string s )
    {
        if ( s == "none" ) return meta::Algorithm::C_None;
        if ( s == "tree" ) return meta::Algorithm::C_NTree;
        if ( s == "ntree" ) return meta::Algorithm::C_NTree;
        throw wibble::exception::OutOfRange( "compression", "'" + s + "' is not a known compression type" ); // TODO: allowed
    }

    graph::DemangleStyle parseDemangle( std::string s ) {
        if ( s == "cpp" ) return graph::DemangleStyle::Cpp;
        if ( s == "none" ) return graph::DemangleStyle::None;
        throw wibble::exception::OutOfRange( "demangle", "'" + s + "' is not supported demangle style [available = none, cpp]" );
    }

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
                 || opts.foundCommand() == compile.cmd_compile )
            {
                return;
            }

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
        meta.input.propertyName = o_property->boolValue() ? o_property->stringValue() : "deadlock";
        meta.input.definitions = o_definitions->values();
        meta.input.probabilistic = o_probabilistic->boolValue();
        meta.output.wantCe = !o_noCe->boolValue();
        meta.algorithm.hashCompaction = o_hashCompaction->boolValue();
        meta.algorithm.sharedVisitor = o_shared->boolValue();
        if ( !o_noreduce->boolValue() ) {
            if ( o_reduce->boolValue() )
                meta.algorithm.reduce = parseReductions( o_reduce->stringValue() );
            else
                meta.algorithm.reduce = parseReductions( "tau+,taustores,heap,por,LU" );
        }
        meta.algorithm.compression = o_compression->boolValue()
            ? parseCompression( o_compression->stringValue() )
            : ( o_compression->isSet()
                    ? meta::Algorithm::C_NTree
                    : meta::Algorithm::C_None );
        meta.algorithm.hashSeed = static_cast< uint32_t >( o_seed->intValue() );
        meta.algorithm.fairness = o_fair->boolValue();
        meta.algorithm.demangle = o_demangle->boolValue()
            ? parseDemangle( o_demangle->stringValue() )
            : ( o_demangle->isSet()
                    ? graph::DemangleStyle::Cpp
                    : graph::DemangleStyle::None );
        meta.output.statistics = o_statistics->boolValue();

        /* No point in generating counterexamples just to discard them. */
        if ( !o_dispCe->boolValue() && !o_report->boolValue() )
            meta.output.wantCe = false;

        meta.output.displayCe = o_dispCe->boolValue();
        meta.algorithm.maxDistance = o_distance->intValue();
        meta.output.filterProgram = o_render->stringValue();
        meta.algorithm.labels = o_labels->boolValue();
        meta.algorithm.traceLabels = o_traceLabels->boolValue();
        meta.algorithm.bfsLayout = o_bfsLayout->boolValue();
        meta.execution.diskFifo = o_diskFifo->boolValue();
        if ( opts.foundCommand() == cmd_simulate )
            meta.input.trace = o_inputTrace->stringValue();
        else
            meta.input.trace = o_drawTrace->stringValue();

        setupLimits();

        meta.output.file = o_drawOutput->isSet()
            ? o_drawOutput->stringValue()
            : o_outputFile->stringValue();
        meta.output.saveStates = !o_noSaveStates->boolValue();


        if ( !meta.input.dummygen && access( input.c_str(), R_OK ) )
            die( "FATAL: cannot open input file " + input + " for reading" );

        {
            Meta metaInfo( meta );
            metaInfo.algorithm.algorithm = meta::Algorithm::Info;
            auto infoAlg = select( metaInfo );
            auto ib = dynamic_cast< InfoBase * >( infoAlg.get() );
            if ( !ib )
                die( "Fatal error encountered while processing input." );

            ib->propertyInfo( meta.input.propertyName, meta );
            meta.algorithm.reduce = ib->filterReductions( meta.algorithm.reduce );
        }

        auto pt = meta.input.propertyType;

        if ( opts.foundCommand() == cmd_draw ) {
            meta.execution.threads = 1; // never runs in parallel
            meta.algorithm.algorithm = meta::Algorithm::Draw;
            meta.algorithm.name = "Draw";
        } else if ( opts.foundCommand() == cmd_simulate ) {
            meta.execution.threads = 1; // never runs in parallel
            meta.algorithm.algorithm = meta::Algorithm::Simulate;
        } else if ( opts.foundCommand() == cmd_compact )
            meta.algorithm.algorithm = meta::Algorithm::Compact;
        else if ( opts.foundCommand() == cmd_info )
            meta.algorithm.algorithm = meta::Algorithm::Info;
        else if ( opts.foundCommand() == cmd_metrics )
            meta.algorithm.algorithm = meta::Algorithm::Metrics;
        else if ( opts.foundCommand() == cmd_verify ) {

            bool oneSet = false;
            for ( auto x : { o_ndfs->boolValue(), o_reachability->boolValue(),
                    o_owcty->boolValue(), o_map->boolValue() } ) {
                if ( oneSet && x )
                    die( "FATAL: only one of --nested-dfs, --owcty, --map,"
                            " --reachability can be specified" );
                oneSet |= x;
            }

            if ( !oneSet ) {
                /* the default algorithms based on property types */
                switch ( pt ) {
                    case generator::PT_Deadlock:
                    case generator::PT_Goal:
                        meta.algorithm.algorithm = meta::Algorithm::Reachability;
                        break;
                    case generator::PT_Buchi:
                        // initialize meta from Mpi, this also calls MPI::init
                        // and creates (singleton) Mpi instance
                        // it is needed so that meta.execution.nodes is valid
                        assert( !mpi );
                        mpi.reset( new Mpi( o_mpi->boolValue() ) );
                        mpiFillMeta( meta );
                        if ( meta.execution.threads > 1 || meta.execution.nodes > 1 )
                            meta.algorithm.algorithm = meta::Algorithm::Owcty;
                        else
                            meta.algorithm.algorithm = meta::Algorithm::Ndfs;
                        break;
                    default:
                        assert_unimplemented();
                }
            }

            if ( o_ndfs->boolValue() ) {
                if ( pt != generator::PT_Buchi )
                    std::cerr << "WARNING: NDFS is only suitable for LTL/Büchi properties." << std::endl;

                meta.algorithm.algorithm = meta::Algorithm::Ndfs;
                if ( !o_workers->boolValue() )
                    meta.execution.threads = 1;
            }

            if ( o_reachability->boolValue() ) {
                if ( pt == generator::PT_Buchi )
                    std::cerr << "WARNING: Reachability is not suitable for checking LTL/Büchi properties."
                              << std::endl;
                meta.algorithm.algorithm = meta::Algorithm::Reachability;
            }

            if ( o_owcty->boolValue() ) {
                if ( pt != generator::PT_Buchi )
                    std::cerr << "WARNING: OWCTY is only suitable for LTL/Büchi properties." << std::endl;
                meta.algorithm.algorithm = meta::Algorithm::Owcty;
            }

            if ( o_map->boolValue() ) {
                if ( pt != generator::PT_Buchi )
                    std::cerr << "WARNING: MAP is only suitable for LTL/Büchi properties." << std::endl;
                meta.algorithm.algorithm = meta::Algorithm::Map;
            }

        }
        else
            die( "FATAL: Internal error in commandline parser." );

        meta.execution.initialTable = 1L << (o_initable->intValue());

        if ( meta.algorithm.sharedVisitor ) {
            if ( meta.algorithm.algorithm != meta::Algorithm::Metrics &&
                 meta.algorithm.algorithm != meta::Algorithm::Reachability &&
                 meta.algorithm.algorithm != meta::Algorithm::Ndfs )
                die( "FATAL: Shared memory hashtables are not yet supported for this algorithm." );
        }

        // ndfs needs a shared table, also Shared visitor have to have size without dividing
        if ( meta.algorithm.algorithm != meta::Algorithm::Ndfs
             && !meta.algorithm.sharedVisitor )
            meta.execution.initialTable /= meta.execution.threads;

        if ( meta.algorithm.algorithm == meta::Algorithm::Ndfs &&
                meta.execution.threads > 1 && !meta.algorithm.sharedVisitor )
        {
            std::cerr << "WARNING: Parallel Nested DFS will use shared hash-table." << std::endl;
            meta.algorithm.sharedVisitor = true;
        }
    }

};

}

int main( int argc, const char **argv )
{
    divine::Main m( argc, argv );
    return 0;
}
