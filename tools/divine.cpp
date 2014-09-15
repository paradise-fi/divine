// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>
//             (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <queue>
#include <iostream>
#include <sstream>
#include <memory>

#include <brick-commandline.h>

#include <wibble/test.h> // for assert
#include <wibble/sys/thread.h>
#include <wibble/sys/mutex.h>
#include <wibble/string.h>
#include <wibble/sfinae.h>
#include <wibble/sys/fs.h>

#include <divine/utility/meta.h>
#include <divine/utility/report.h>
#include <divine/utility/die.h>
#include <divine/instances/select.h>
#include <divine/utility/statistics.h>

#include <tools/combine.h>
#include <tools/compile.h>
#include <tools/info.h>

#ifdef POSIX
#include <sys/resource.h>
#endif

using namespace wibble;
using namespace brick;
using namespace brick::commandline;

namespace divine {

std::shared_ptr< Report > _report = 0;
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
        _report->final( *_meta );
    }
    raise( s );
}

struct Main {
    Output *output;
    std::shared_ptr< Report > report;
    Meta meta;

    Engine *cmd_verify, *cmd_metrics, *cmd_compile, *cmd_draw, *cmd_info,
           *cmd_simulate, *cmd_genexplicit;
    OptionGroup *common, *drawing, *input, *reduce, *compression, *definitions,
                *ce, *compactOutput, *limits;
    BoolOption *o_noCe, *o_dispCe, *o_simulateCe, *o_dummy, *o_statistics, *o_shortReport;
    OptvalStringVectorOption *o_report;
    BoolOption *o_diskFifo;
    BoolOption *o_fair, *o_hashCompaction, *o_shared;
    StringOption *o_reduce;
    OptvalStringOption *o_compression;
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
    StringOption *o_inputTrace, *o_interactiveInputTrace;
    OptvalStringOption *o_demangle;
    StringOption *o_outputFile;
    BoolOption *o_noSaveStates;
    IntOption *o_contextSwitchLimit;

    BoolOption *o_ndfs, *o_map, *o_owcty, *o_reachability, *o_weakReachability, *o_csdr;
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
          opts( "DIVINE", versionString(), 1, "DIVINE Team <divine@fi.muni.cz>" ),
          combine( opts ),
          compile( opts )
    {
        sysinfo::Info::init();
        setupSignals();
        setupCommandline();
        parseCommandline();

        {
            std::ostringstream execCommStr;
            for( int i = 0; i < argc; ++i )
                execCommStr << argv[i] << " ";
            report = getReport();
            report->execCommand( execCommStr.str() );
        }

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
                = new std::ofstream( o_gnuplot->value().c_str() );
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
            if ( o_report->isSet() || o_shortReport->boolValue() )
                _report = report;
        }

        TrackStatistics::global().setup( a->meta() );
        if ( meta.output.statistics )
            TrackStatistics::global().start();


        mpi->start();

        if ( mpi->master() && opts.foundCommand() != cmd_info ) {
            auto a = meta.algorithm.name;
            std::cerr << " input: " << meta.input.model << std::endl
                      << " property " << wibble::str::fmt( meta.input.properties )
                      << ": " << meta.input.propertyDetails << std::endl
                      << " " << std::string( (44 - a.size()) / 2, '-' )
                      << " " << a << " " << std::string( (44 - a.size()) / 2 , '-' )
                      << std::endl;
        }

        a->run();

        report->finished();

        if ( meta.output.statistics )
            TrackStatistics::global().snapshot();
        Output::output().cleanup();
        if ( mpi->master() && (o_report->isSet() || o_shortReport->boolValue())  )
            report->final( a->meta() );

        if ( mpi->master() && o_simulateCe->boolValue()
                && a->meta().result.propertyHolds == meta::Result::R::No )
        {
            Meta copy = a->meta();
            copy.algorithm.algorithm = meta::Algorithm::Type::Simulate;
            auto simulate = select( copy );

            std::cerr << std::endl << "Counterexample found, running simulate..." << std::endl;
            simulate->run();
        }
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
        cmd_genexplicit = opts.addEngine( "gen-explicit",
                                          "<input>",
                                          "convert state-space to explicit representation" );

        common = opts.createGroup( "Common Options" );
        drawing = opts.createGroup( "Drawing Options" );
        input = opts.createGroup( "Input Options" );
        reduce = opts.createGroup( "Reduction Options" );
        compression = opts.createGroup( "Compression Options" );
        definitions = opts.createGroup( "Definitions" );
        ce = opts.createGroup( "Counterexample Options" );
        compactOutput = opts.createGroup( "Output option" );
        limits = opts.createGroup( "Resource limits" );

        o_curses = opts.add< BoolOption >(
            "curses", '\0', "curses", "", "use curses-based progress monitoring" );

        o_shortReport = common->add< BoolOption >(
            "report", 'r', "", "", "output standardised report" );

        o_report = common->add< OptvalStringVectorOption >(
            "report", '\0', "report", "", "output report, one of:\n"
            "text (stdout, human readable),\n"
            "text:<filename> (text into file),\n"
            "plain (stdout, same as text but without empty lines),\n"
            "plain:<filename> (plain into file),\n"
            "sql:<table>:<ODBC connection string> (write report to database -- see manual for details)" );

        o_workers = common->add< IntOption >(
            "workers", 'w', "workers", "",
            "number of worker threads (default: 2)" );

        o_mem = limits->add< IntOption >(
            "max-memory", '\0', "max-memory", "",
            "maximum memory to use in MB (default: 0 = unlimited)" );

        o_time = limits->add< IntOption >(
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

        o_compression = compression->add< OptvalStringOption >(
            "compression", '\0', "compression", "",
            "configure state space compression; available: none, tree; "
            "default is none if --compression is not specified, tree otherwise" );

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

        o_demangle = common->add< OptvalStringOption >(
            "demangle", 0, "demangle", "",
            "Demagle style of symbols (only for LLVM verification), "
            "available=node,cpp; default is none if --demangle in not "
            "specified, cpp otherwise" );

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

        o_simulateCe = ce->add< BoolOption >(
            "simulate-counterexample", '\0', "simulate-counterexample", "",
            "run simulate with counterexample after finishing (if CE is found)" );

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
        o_weakReachability = cmd_verify->add< BoolOption >(
            "weak-reachability", 0, "weak-reachability", "", "force weak reachability" );
        o_csdr = cmd_verify->add< BoolOption >(
            "csdr", 0, "csdr", "", "force Context Switch Driven Reachability" );

        o_contextSwitchLimit = cmd_verify->add< IntOption >(
            "context-switch-limit", 0, "context-switch-limit", "",
            "Specify maximal allowed number of context-switches. "
            "Impiles CSDR algorithm, not supported for LTL. 0 means unlimited. [default = 0]" );

        // simulate options
        o_inputTrace = cmd_simulate->add< StringOption >(
            "trace", 't', "trace", "",
            "generate states of a numeric trace and exit" );
        o_interactiveInputTrace = cmd_simulate->add< StringOption >(
            "interactive-trace", 'T', "interactive-trace", "",
            "generate states of a numeric trace and continue in interactive mode" );

        o_outputFile = compactOutput->add< StringOption >(
                "output", 'o', "output", "",
                "the output file name (if not specified <current-dir>/<model-name>.dess is used)" );
        o_noSaveStates = compactOutput->add< BoolOption >(
                "no-save-states", 0, "no-save-states", "",
                "do not save states in DCESS file, only save transitions." );

        cmd_metrics->add( common );
        cmd_metrics->add( limits );
        cmd_metrics->add( reduce );
        cmd_metrics->add( compression );
        cmd_metrics->add( input );
        cmd_metrics->add( definitions );

        cmd_verify->add( common );
        cmd_verify->add( limits );
        cmd_verify->add( ce );
        cmd_verify->add( reduce );
        cmd_verify->add( compression );
        cmd_verify->add( input );
        cmd_verify->add( definitions );

        cmd_simulate->add( common );
        cmd_simulate->add( limits );
        cmd_simulate->add( reduce );
        cmd_simulate->add( compression );
        cmd_simulate->add( definitions );

        cmd_genexplicit->add( common );
        cmd_genexplicit->add( limits );
        cmd_genexplicit->add( reduce );
        cmd_genexplicit->add( compression );
        cmd_genexplicit->add( definitions );
        cmd_genexplicit->add( input );
        cmd_genexplicit->add( compactOutput );

        cmd_draw->add( drawing );
        cmd_draw->add( limits );
        cmd_draw->add( reduce );
        cmd_draw->add( o_property );
        cmd_draw->add( compression );
        cmd_draw->add( input );
        cmd_draw->add( definitions );

        opts.setPartialMatchingRecursively( true );
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

    std::shared_ptr< Report > getOneReport( std::string repStr ) {
        std::shared_ptr< Report > rep;

        if ( repStr == "" || repStr == "text" )
            rep = Report::get< TextReport >( std::cout );
        else if ( repStr == "plain" )
            rep = Report::get< PlainReport >( std::cout );
        else if ( repStr.substr( 0, 5 ) == "text:" ) {
            std::string file = repStr.substr( 5 );
            if ( file.empty() )
                throw wibble::exception::Consistency( "No file given for report." );
            rep = Report::get< TextFileReport >( file );
        } else if ( repStr.substr( 0, 6 ) == "plain:" ) {
            std::string file = repStr.substr( 6 );
            if ( file.empty() )
                throw wibble::exception::Consistency( "No file given for report." );
            rep = Report::get< PlainFileReport >( file );
        } else if ( repStr.substr( 0, 4 ) == "sql:" ) {
            std::string sqlrep = repStr.substr( 4 );
            int pos = sqlrep.find( ':' );
            std::string db = sqlrep.substr( 0, pos );
            std::string connstr = sqlrep.substr( pos + 1 );
            if ( connstr.empty() )
                throw wibble::exception::Consistency( "No connection string given for report." );
            rep = Report::get< SqlReport >( db, connstr );
        }

        if ( !rep )
            throw wibble::exception::Consistency( "Unknown or unsupported report: " + repStr );

        return rep;
    }

    std::shared_ptr< Report > getReport() {
        std::shared_ptr< Report > rep;
        if ( o_report->isSet() ) {
            auto values = o_report->values();
            if ( o_report->emptyValueSet() )
                values.push_back( "" );
            assert_leq( 1UL, values.size() );

            if ( values.size() > 1 ) {
                rep = Report::get< AggregateReport >();
                auto agreg = std::dynamic_pointer_cast< AggregateReport >( rep );
                for ( auto s : values )
                    agreg->addReport( getOneReport( s ) );
            } else
                rep = getOneReport( values[ 0 ] );

        } else if ( o_shortReport->isSet() )
            rep = Report::get< TextReport >( std::cout );

        if ( !rep )
            rep = Report::get< NoReport >();

        return rep;
    }

    graph::PropertySet parseProperties( std::string s )
    {
        wibble::str::Split splitter( ",", s );
        graph::PropertySet r;
        std::copy( splitter.begin(), splitter.end(), std::inserter( r, r.begin() ) );
        return r;
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

    meta::Algorithm::Compression parseCompression( std::string s )
    {
        if ( s.empty() ) return meta::Algorithm::Compression::Tree;
        if ( s == "none" ) return meta::Algorithm::Compression::None;
        if ( s == "tree" ) return meta::Algorithm::Compression::Tree;
        if ( s == "ntree" ) return meta::Algorithm::Compression::Tree;
        throw wibble::exception::OutOfRange( "compression", "'" + s
                + "' is not a known compression type" ); // TODO: allowed
    }

    graph::DemangleStyle parseDemangle( std::string s ) {
        if ( s.empty() ) return graph::DemangleStyle::Cpp;
        if ( s == "cpp" ) return graph::DemangleStyle::Cpp;
        if ( s == "none" ) return graph::DemangleStyle::None;
        throw wibble::exception::OutOfRange( "demangle", "'" + s
                + "' is not supported demangle style [available = none, cpp]" );
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

        } catch( commandline::BadOption &e ) {
            die( e.what() );
        }

        if ( !opts.foundCommand() )
            die( "FATAL: no command specified" );

        if ( o_simulateCe->boolValue() && o_weakReachability->boolValue() )
            die( "FATAL: --simulate-counterexample cannot be used with --weak-reachability,\n"
                 "       use --reachability instead" );

        if ( o_workers->boolValue() )
            meta.execution.threads = o_workers->intValue();
        // else default (currently set to 2)

        meta.input.model = input;
        meta.input.properties = parseProperties( o_property->boolValue() ? o_property->value() : "deadlock" );
        meta.input.definitions = o_definitions->values();
        meta.input.probabilistic = o_probabilistic->boolValue();
        meta.output.wantCe = !o_noCe->boolValue();
        meta.algorithm.hashCompaction = o_hashCompaction->boolValue();
        meta.algorithm.sharedVisitor = o_shared->boolValue();
        if ( !o_noreduce->boolValue() ) {
            if ( o_reduce->boolValue() )
                meta.algorithm.reduce = parseReductions( o_reduce->value() );
            else
                meta.algorithm.reduce = parseReductions( "tau+,taustores,heap,por,LU" );
        }
        meta.algorithm.compression = o_compression->isSet()
            ? parseCompression( o_compression->value() )
            : meta::Algorithm::Compression::None;
        meta.algorithm.hashSeed = static_cast< uint32_t >( o_seed->intValue() );
        meta.algorithm.fairness = o_fair->boolValue();
        meta.algorithm.demangle = o_demangle->isSet()
            ? parseDemangle( o_demangle->value() )
            : graph::DemangleStyle::None;
        meta.output.statistics = o_statistics->boolValue();

        /* No point in generating counterexamples just to discard them. */
        if ( !o_dispCe->boolValue() && !o_simulateCe->boolValue()
                && !o_report->isSet() && !o_shortReport->boolValue() )
            meta.output.wantCe = false;

        meta.output.displayCe = o_dispCe->boolValue();
        meta.algorithm.maxDistance = o_distance->intValue();
        meta.output.filterProgram = o_render->value();
        meta.algorithm.labels = o_labels->boolValue();
        meta.algorithm.traceLabels = o_traceLabels->boolValue();
        meta.algorithm.bfsLayout = o_bfsLayout->boolValue();
        meta.execution.diskFifo = o_diskFifo->boolValue();
        if ( opts.foundCommand() == cmd_simulate ) {
            if ( o_inputTrace->isSet() && o_interactiveInputTrace->isSet() )
                die( "Use just one of --trace / --int-trace" );
            meta.input.trace = o_inputTrace->isSet()
                ? o_inputTrace->value()
                : o_interactiveInputTrace->value();
            meta.algorithm.interactive = !o_inputTrace->isSet();
        }
        else
            meta.input.trace = o_drawTrace->value();

        setupLimits();

        meta.output.file = o_drawOutput->isSet()
            ? o_drawOutput->value()
            : o_outputFile->value();
        meta.output.saveStates = !o_noSaveStates->boolValue();


        if ( !meta.input.dummygen && access( input.c_str(), R_OK ) )
            die( "FATAL: cannot open input file " + input + " for reading" );

        {
            Meta metaInfo( meta );
            metaInfo.algorithm.algorithm = meta::Algorithm::Type::Info;
            auto infoAlg = select( metaInfo );
            auto ib = dynamic_cast< InfoBase * >( infoAlg.get() );
            if ( !ib )
                die( "Fatal error encountered while processing input." );

            ib->propertyInfo( meta.input.properties, meta );
            meta.algorithm.reduce = ib->filterReductions( meta.algorithm.reduce );
        }

        auto pt = meta.input.propertyType;

        if ( opts.foundCommand() == cmd_draw ) {
            meta.execution.threads = 1; // never runs in parallel
            meta.algorithm.algorithm = meta::Algorithm::Type::Draw;
            meta.algorithm.name = "Draw";
        } else if ( opts.foundCommand() == cmd_simulate ) {
            meta.execution.threads = 1; // never runs in parallel
            meta.algorithm.algorithm = meta::Algorithm::Type::Simulate;
        } else if ( opts.foundCommand() == cmd_genexplicit )
            meta.algorithm.algorithm = meta::Algorithm::Type::GenExplicit;
        else if ( opts.foundCommand() == cmd_info )
            meta.algorithm.algorithm = meta::Algorithm::Type::Info;
        else if ( opts.foundCommand() == cmd_metrics )
            meta.algorithm.algorithm = meta::Algorithm::Type::Metrics;
        else if ( opts.foundCommand() == cmd_verify ) {

            bool oneSet = false;
            for ( auto x : { o_ndfs->boolValue(), o_reachability->boolValue(),
                    o_weakReachability->boolValue(), o_csdr->boolValue(),
                    o_owcty->boolValue(), o_map->boolValue() } )
            {
                if ( oneSet && x )
                    die( "FATAL: only one of --nested-dfs, --owcty, --map,"
                            " --reachability, --weak-reachability can be specified" );
                oneSet |= x;
            }

            if ( o_contextSwitchLimit->boolValue() ) {
                meta.algorithm.contextSwitchLimit = o_contextSwitchLimit->intValue();
                if ( oneSet ) {
                    if ( !o_csdr->boolValue() )
                        die( "Algorithm other then CSDR specified with --context-switch-limit" );
                    if ( pt == generator::PT_Buchi )
                        die( "LTL/Büchi properties are not supported with --context-switch-limit" );
                } else
                    meta.algorithm.algorithm = meta::Algorithm::Type::Csdr;
            }

            if ( !oneSet ) {
                /* the default algorithms based on property types */
                switch ( pt ) {
                    case generator::PT_Deadlock:
                    case generator::PT_Goal:
                        meta.algorithm.algorithm = meta::Algorithm::Type::Reachability;
                        break;
                    case generator::PT_Buchi:
                        // initialize meta from Mpi, this also calls MPI::init
                        // and creates (singleton) Mpi instance
                        // it is needed so that meta.execution.nodes is valid
                        assert( !mpi );
                        mpi.reset( new Mpi( o_mpi->boolValue() ) );
                        mpiFillMeta( meta );
#if ALG_NDFS
                        if ( meta.execution.threads == 1 && meta.execution.nodes == 1 )
                            meta.algorithm.algorithm = meta::Algorithm::Type::Ndfs;
                        else
#endif
                            meta.algorithm.algorithm = meta::Algorithm::Type::Owcty;
                        break;
                    default:
                        assert_unimplemented();
                }
            }

            if ( o_ndfs->boolValue() ) {
                if ( pt != generator::PT_Buchi )
                    std::cerr << "WARNING: NDFS is only suitable for LTL/Büchi properties." << std::endl;

                meta.algorithm.algorithm = meta::Algorithm::Type::Ndfs;
                if ( !o_workers->boolValue() )
                    meta.execution.threads = 1;
            }

            if ( o_reachability->boolValue() ) {
                if ( pt == generator::PT_Buchi )
                    std::cerr << "WARNING: Reachability is not suitable for checking LTL/Büchi properties."
                              << std::endl;
                meta.algorithm.algorithm = meta::Algorithm::Type::Reachability;
            }

            if ( o_weakReachability->boolValue() ) {
                if ( pt == generator::PT_Buchi )
                    std::cerr << "WARNING: Weak reachability is not suitable for checking LTL/Büchi properties."
                              << std::endl;
                meta.algorithm.algorithm = meta::Algorithm::Type::WeakReachability;
            }

            if ( o_csdr->boolValue() ) {
                if ( pt == generator::PT_Buchi )
                    std::cerr << "WARNING: Context Switch Driven Reachability is not suitable for checking LTL/Büchi properties."
                              << std::endl;
                meta.algorithm.algorithm = meta::Algorithm::Type::Csdr;
            }

            if ( o_owcty->boolValue() ) {
                if ( pt != generator::PT_Buchi )
                    std::cerr << "WARNING: OWCTY is only suitable for LTL/Büchi properties." << std::endl;
                meta.algorithm.algorithm = meta::Algorithm::Type::Owcty;
            }

            if ( o_map->boolValue() ) {
                if ( pt != generator::PT_Buchi )
                    std::cerr << "WARNING: MAP is only suitable for LTL/Büchi properties." << std::endl;
                meta.algorithm.algorithm = meta::Algorithm::Type::Map;
            }

        }
        else
            die( "FATAL: Internal error in commandline parser." );

        meta.execution.initialTable = 1L << (o_initable->intValue());

        if ( meta.algorithm.sharedVisitor ) {
            if ( meta.algorithm.algorithm != meta::Algorithm::Type::Metrics &&
                 meta.algorithm.algorithm != meta::Algorithm::Type::Reachability &&
                 meta.algorithm.algorithm != meta::Algorithm::Type::WeakReachability &&
                 meta.algorithm.algorithm != meta::Algorithm::Type::Csdr &&
                 meta.algorithm.algorithm != meta::Algorithm::Type::Owcty &&
                 meta.algorithm.algorithm != meta::Algorithm::Type::Map &&
                 meta.algorithm.algorithm != meta::Algorithm::Type::Ndfs )
                die( "FATAL: Shared memory hashtables are not yet supported for this algorithm." );
        }

        // ndfs needs a shared table, also Shared visitor have to have size without dividing
        if ( meta.algorithm.algorithm != meta::Algorithm::Type::Ndfs
             && !meta.algorithm.sharedVisitor )
            meta.execution.initialTable /= meta.execution.threads;

        if ( meta.algorithm.algorithm == meta::Algorithm::Type::Ndfs &&
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
    try {
        divine::Main m( argc, argv );
    } catch ( divine::DieException &ex ) {
        std::cerr << ex.desc() << std::endl;
        std::cerr << "Exiting after receiving fatal error." << std::endl;
        std::exit( ex.exitcode );
    } catch ( wibble::exception::Generic &ex ) {
        std::cerr << "FATAL ERROR: caught error during verification:" << std::endl
                  << "    " << ex.what() << std::endl;
        std::cerr << "Exiting after receiving fatal error." << std::endl;
        exit( 2 );
    } catch ( std::bad_alloc & ) {
        std::cerr << "FATAL ERROR: failed to allocate memory, exiting." << std::endl;
        exit( 3 );
    }
    return 0;
}
