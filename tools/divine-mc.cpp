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
#include <divine/reachability.h>
#include <divine/owcty.h>
#include <divine/map.h>
#include <divine/ndfs.h>
#include <divine/ndfs-vcl.h>
#include <divine/generator.h>

#include <divine/report.h>

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

    Engine *cmd_reachability, *cmd_owcty, *cmd_ndfs, *cmd_map, *cmd_verify;
    OptionGroup *common, *order;
    BoolOption *o_dfs, *o_bfs, *o_shared, *o_verbose, *o_vcl, *o_pool,
        *o_por, *o_noCe, *o_report;
    IntOption *o_store, *o_grow, *o_handoff, *o_workers, *o_locking, *o_mem;
    StringOption *o_trail;

    bool dummygen;

    int argc;
    const char **argv;
    commandline::StandardParserWithMandatoryCommand opts;

    Main( int _argc, const char **_argv )
        : dummygen( false ), argc( _argc ), argv( _argv ),
          opts( argv[ 0 ], DIVINE_VERSION, 1, "DiVinE Team <divine@fi.muni.cz>" )
    {
        setupSignals();
        setupCommandline();
        parseCommandline();

        Report rep( config );
        report = &rep;
        rep.start();
        rep.finished( runController() );
        rep.final( std::cout );
    }

    void die( std::string bla )
    {
        std::cerr << bla << std::endl;
        exit( 1 );
    }

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
        opts.description = "Multi-Threaded Model Checker based on DiVinE";

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
        order = opts.createGroup( "Visit Order Options" );

        o_verbose = common->add< BoolOption >(
            "verbose", 'v', "verbose", "", "more verbose operation" );
        o_report = common->add< BoolOption >(
            "report", 'r', "report", "", "output standardised report" );

        o_workers = common->add< IntOption >(
            "workers", 'w', "workers", "",
            "number of worker threads (default: 2)" );
        o_workers ->setValue( 2 );

        o_store = common->add< IntOption >(
            "init-store", 'i', "init-store", "",
            "initial store (hash table) size (default: 4097)" );
        o_store->setValue( 4097 );

        o_grow = common->add< IntOption >(
            "store-grow", 'g', "store-grow", "",
            "store grow factor (default: 2)" );
        o_grow->setValue( 2 );

        o_mem = common->add< IntOption >(
            "max-memory", '\0', "max-memory", "",
            "maximum memory to use in MB (default: 0 = unlimited)" );

        o_handoff = common->add< IntOption >(
            "handoff", '\0', "handoff", "",
            "handoff threshold (default: 50)" );
        o_handoff->setValue( 50 );

        o_shared = common->add< BoolOption >(
            "shared", 's', "shared", "",
            "use shared storage instead of partitioning" );

        o_pool = common->add< BoolOption >(
            "disable-pool", '\0', "disable-pool", "",
            "disable pooled allocation (use HOARD for all allocation)" );

        o_noCe = common->add< BoolOption >(
            "no-counterexample", 'n', "no-counterexample", "",
            "disable counterexample generation" );

        /* o_por = common->add< BoolOption >(
            "por", 'p', "por", "",
            "use partial order reduction" ); */

        o_locking = common->add< IntOption >(
            "locking", '\0', "locking", "",
            "locking scheme to use; 0 = no locking, 1 = const, "
            "2 = sqrt, 3 = linear, 4 = naive (default: 2)" );
        o_locking->setValue( 1 );

        o_trail = common->add< StringOption >(
            "trail", 't', "trail", "",
            "file to output trail to (default: input-file.trail)" );

        o_bfs = order->add< BoolOption >(
            "bfs", 'b', "bfs", "", "Breadth-First Search (default)" );
        o_dfs = order->add< BoolOption >(
            "dfs", 'd', "dfs", "", "Depth-First Search" );

        /* o_vcl = cmd_ndfs->add< BoolOption >(
            "vcl", 'v', "vcl", "", "VCL decomposition (warning:
            disobeys -t)"); */

        opts.add( common );
        cmd_reachability->add( order );
        cmd_owcty->add( order );
        cmd_map->add( order );
    }

    enum { RunReachability, RunNdfs, RunMap, RunOwcty } m_run;

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

        config.setMaxThreads( o_workers->intValue() + 1 );
        config.setStorageInitial( o_store->intValue() );
        config.setStorageFactor( o_grow->intValue() );
        config.setHandoff( o_handoff->intValue() );
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
            std::string t = string( input, 0,
                                    input.rfind( '.' ) );
            t += ".trail";
            config.setTrailFile( t );
        } else
            config.setTrailFile( o_trail->stringValue() );

        if ( o_dfs->boolValue() && o_bfs->boolValue() )
            die( "FATAL: only one of --dfs and --bfs may be used" );

        if ( !dummygen && access( input.c_str(), R_OK ) )
            die( "FATAL: cannot open input file " + input + " for reading" );

        if ( !opts.foundCommand() )
            die( "FATAL: no command specified" );

        if ( opts.foundCommand() == cmd_verify ) {
            std::string inf = fs::readFile( config.input() );
            if ( inf.find( "system async property" ) != std::string::npos ||
                 inf.find( "system sync property" ) != std::string::npos ) {
                // we have a property automaton --> LTL
                if ( config.maxThreadCount() > 2 ) {
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
        else
            die( "FATAL: Internal error in commandline parser." );

        if ( config.verbose() ) {
            std::cerr << " === configured options ===" << std::endl;
            config.dump( std::cerr );
        }
    }

    Result runController()
    {
        if ( m_run == RunNdfs ) {
            config.setPartitioning( "None" );
            return runStorage< controller::Simple >();
        } else {
            if ( o_shared->boolValue() ) {
                if ( o_dfs->boolValue() ) {
                    config.setPartitioning( "Handoff" );
                    return runStorage< controller::Handoff >();
                } else {
                    config.setPartitioning( "Modular" );
                    return runStorage< controller::Shared >();
                }
            } else {
                config.setPartitioning( "Static" );
                return runStorage< controller::Partition >();
            }
        }

        return Result();
    }

    template< typename Cont >
    Result runStorage()
    {
        if ( o_shared->boolValue() ) {
            if ( o_locking->intValue() == 0 ) {
                config.setStorage( "Shared-Lockless" );
                return runOrder< Cont, storage::LocklessShared >();
            } else if ( o_locking->intValue() == 1 ) {
                config.setStorage( "Shared-Constlock" );
                return runOrder< Cont, storage::ConstShared >();
            } else if ( o_locking->intValue() == 2 ) {
                config.setStorage( "Shared-Sqrtlock" );
                return runOrder< Cont, storage::SqrtShared >();
            } else if ( o_locking->intValue() == 3 ) {
                config.setStorage( "Shared-Linearlock" );
                return runOrder< Cont, storage::LinearShared >();
            } else if ( o_locking->intValue() == 4 ) {
                config.setStorage( "Shared-Naivelock" );
                return runOrder< Cont, storage::NaiveShared >();
            }
        } else {
            if ( o_pool->boolValue() ) {
                config.setStorage( "Unpooled-Partitioned" );
                return runOrder< Cont, storage::Partition >();
            } else {
                config.setStorage( "Pooled-Partitioned" );
                return runOrder< Cont, storage::PooledPartition >();
            }
        }

        return Result();
    }

    template< typename Cont, typename Stor >
    Result runOrder()
    {
        if ( m_run == RunNdfs ) {
            config.setOrder( "DFS" );
            return runAlgorithm< Cont, Stor, visitor::DFS >();
        }

        /* if ( o_por->boolValue() ) {
            verbose( "partial order reduction: enabled" );
            if ( o_dfs->boolValue() ) {
                verbose( "search order: depth first" );
                runAlgorithm< Cont, Stor, visitor::PorDFS >();
            } else {
                verbose( "search order: breadth first" );
                runAlgorithm< Cont, Stor, visitor::PorBFS >();
            }
        } else */ { 
            // verbose( "partial order reduction: disabled" );
            if ( o_dfs->boolValue() ) {
                config.setOrder( "DFS" );
                return runAlgorithm< Cont, Stor, visitor::DFS >();
            } else {
                config.setOrder( "BFS" );
                return runAlgorithm< Cont, Stor, visitor::BFS >();
            }
        }

        return Result();
    }

    template< typename Cont, typename Stor, typename Ord >
    Result runAlgorithm()
    {
        if ( m_run == RunReachability ) {
            config.setAlgorithm( "Reachability" );
            return runGenerator< algorithm::Reachability, Cont, Stor, Ord >();
        }

        if ( m_run == RunOwcty ) {
            config.setAlgorithm( "OWCTY" );
            return runGenerator< algorithm::Owcty, Cont, Stor, Ord >();
        }

        if ( m_run == RunMap ) {
            config.setAlgorithm( "MAP" );
            return runGenerator< algorithm::Map, Cont, Stor, Ord >();
        }

        if ( m_run == RunNdfs ) {
            config.setAlgorithm( "NestedDFS" );
            return runGenerator< algorithm::NestedDFS, Cont, Stor, Ord >();
        }

        return Result();
    }

    template< template< typename > class Alg, typename Cont, typename Stor,
              typename Ord >
    Result runGenerator()
    {
        if ( str::endsWith( config.input(), ".dve" ) ) {
            config.setGenerator( "DiVinE" );
            return run< generator::Dve, Alg, Cont, Stor, Ord >( Preferred() );
        } else if ( str::endsWith( config.input(), ".b" ) ) {
            config.setGenerator( "NIPS" );
            return run< generator::Bymoc, Alg, Cont, Stor, Ord >( Preferred() );
        } else if ( dummygen ) {
            config.setGenerator( "Dummy" );
            return run< generator::Dummy, Alg, Cont, Stor, Ord >( Preferred() );
        } else
	    die( "Error: The input file extension is unknown." );
        return Result();
    }

    /* this here prunes the set of valid combinations */
    template< template< typename > class G, template< typename > class A,
              typename C, typename S, typename O >
    typename EnableIf<
        TAnd<
            TOr<
                TAnd<
                    TSame< C, controller::Partition >,
                    TSame< S, storage::Partition > >,
                TAnd<
                    TSame< C, controller::Partition >,
                    TSame< S, storage::PooledPartition > >,
                TAnd<
                    TSame< A< Unit >, algorithm::NestedDFS< Unit > >,
                    TSame< C, controller::Simple >,
                    TSame< O, visitor::DFS > >,
                TAnd< TOr< TSame< C, controller::Shared >,
                           TSame< C, controller::Handoff > >,
                      storage::IsShared< S > >
                >,
            TImply< TSame< C, controller::Handoff >, TSame< O, visitor::DFS > >,
            TImply< TSame< C, controller::Simple >,
                    TSame< A< Unit >, algorithm::NestedDFS< Unit > > >,
            TImply< TSame< A< Unit >, algorithm::NestedDFS< Unit > >,
                    TSame< O, visitor::DFS > >
        >, Result >::T
    run( Preferred ) {
        return doRun< G, A, C, S, O >( Preferred() );
    }

    /* This only includes a fairly reasonable set of combinations. */
    template< template< typename > class G, template< typename > class A,
              typename C, typename S, typename O >
    typename EnableIf< TAnd<
                           TSame< S, storage::PooledPartition >,
                           TImply<
                               TSame< O, visitor::DFS >,
                               TSame< A< Unit >, algorithm::NestedDFS< Unit > >
                           >,
                           TSame< G< Unit >, generator::Dve< Unit > >,
                           TOr< TSame< A< Unit >, algorithm::Owcty< Unit > >,
                                TSame< A< Unit >, algorithm::NestedDFS< Unit > > >
                           /* TOr< TSame< G< Unit >, generator::Dve< Unit > >,
                              TSame< G< Unit >, generator::Bymoc< Unit > > > */ >,
        Result >::T
    doRun( Preferred ) {
        A< Algorithm::Setup< C, S, O, G > > alg( config );
        return alg.run();
    }

    template< template< typename > class G, template< typename > class A,
              typename C, typename S, typename O >
    Result doRun( NotPreferred ) {
        std::cerr << "FATAL: selected combination is not supported by "
                  << "this binary" << std::endl;
        return Result();
    }

    /* this here is called if the combination does not satisfy the
       predicate in the above instance of run( Preferred ) */
    template< template< typename > class G, template< typename > class A,
              typename C, typename S, typename O >
    Result run( NotPreferred )
    {
        std::cerr << "FATAL: illegal combination of features selected"
                  << std::endl;
        return Result();
    }

};


int main( int argc, const char **argv )
{
    Main m( argc, argv );
    return 0;
}
