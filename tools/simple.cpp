// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>
#include <iostream>

#include <wibble/commandline/parser.h>
#include <wibble/sys/fs.h>

#include <divine/config.h>
#include <divine/generator/legacy.h>
#include <divine/algorithm/simple.h>
#include <divine/report.h>

using namespace divine;
using namespace wibble;
using namespace commandline;

struct Main {
    Config config;

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

        Report report( config );
        Result res;

        config.setAlgorithm( "Reachability" );
        config.setGenerator( "DVE" );

        try {
            algorithm::Simple< generator::NDve > alg( &config );
            alg.domain().mpi.init();
            alg.domain().mpi.start();
            report.mpiInfo( alg.domain().mpi );

            res = alg.run();
        } catch (std::exception &e) {
            die( std::string( "FATAL: " ) + e.what() );
        }

        report.finished( res );
        report.final( std::cout );
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

        config.setWorkers( o_workers->intValue() );
        config.setInitialTableSize(
            ( 2 << (o_initable->intValue()) ) / o_workers->intValue() );
        config.setInput( input );
        config.setReport( o_report->boolValue() );

        if ( access( input.c_str(), R_OK ) )
            die( "FATAL: cannot open input file " + input + " for reading" );
    }
};

int main( int argc, const char **argv )
{
    Main m( argc, argv );
    return 0;
}
