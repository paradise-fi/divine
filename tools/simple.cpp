// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>
#include <iostream>

#include <wibble/commandline/parser.h>
#include <wibble/sys/fs.h>

#include <divine/meta.h>
#include <divine/generator/legacy.h>
#include <divine/algorithm/simple.h>
#include <divine/report.h>

using namespace divine;
using namespace wibble;
using namespace commandline;

struct Main {
    Meta meta;

    BoolOption *o_report;
    IntOption *o_workers, *o_initable;

    int argc;
    const char **argv;
    commandline::StandardParserWithManpage opts;

    Main( int _argc, const char **_argv )
        : argc( _argc ), argv( _argv ),
          opts( "DiVinE Simple", versionString(), 1, "DiVinE Team <divine@fi.muni.cz>" )
    {
        setupCommandline();
        parseCommandline();

        Report report;

        meta.algorithm.name = "Simple Reachability";
        meta.input.modelType = "Legacy DVE";

        try {
            Mpi mpi;
            struct Setup {
                template< typename X > using Topology = Topology<>::Mpi< X >;
                using Statistics = NoStatistics;
                using Graph = generator::LegacyDve;
                using Store = visitor::PartitionedStore< Graph, algorithm::Hasher >;
            };
            algorithm::Simple< Setup > alg( meta, true );

            meta.execution.nodes = mpi.size();
            meta.execution.thisNode = mpi.rank();
            mpi.start();

            alg.run();
            report.finished();
            if ( o_report->boolValue() )
                report.final( std::cout, alg.meta() );
        } catch (std::exception &e) {
            die( std::string( "FATAL: " ) + e.what() );
        }

    }

    static void die( std::string bla ) __attribute__((noreturn))
    {
        std::cerr << bla << std::endl;
        exit( 1 );
    }

    void setupCommandline()
    {
        opts.usage = "<command> [command-specific options] <input file>";
        opts.description = "A Parallel State-Space Exploration tool for DVE";

        o_report = opts.add< BoolOption >(
            "report", 'r', "report", "", "output standardised report" );

        o_workers = opts.add< IntOption >(
            "workers", 'w', "workers", "",
            "number of worker threads (default: 2)" );
        o_workers ->setValue( 2 );

        o_initable = opts.add< IntOption >(
            "initial-table", 'i', "initial-table", "",
            "set initial hash table size to 2^n [default = 19]" );
        o_initable ->setValue( 19 );
    }

    void parseCommandline()
    {
        std::string input;

        try {
            if ( opts.parse( argc, argv ) ) {
                exit( 0 ); // built-in command executed
            }

            if ( !opts.hasNext() ) {
                die( "FATAL: no input file specified" );
            } else {
                input = opts.next();
            }

        } catch( wibble::exception::BadOption &e ) {
            die( e.fullInfo() );
        }

        meta.execution.threads = o_workers->intValue();
        meta.execution.initialTable =
            ( 2 << (o_initable->intValue()) ) / o_workers->intValue();
        meta.input.model = input;

        if ( access( input.c_str(), R_OK ) )
            die( "FATAL: cannot open input file " + input + " for reading" );
    }
};

int main( int argc, const char **argv )
{
    Main m( argc, argv );
    return 0;
}
