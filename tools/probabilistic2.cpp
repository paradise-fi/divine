// -*- C++ -*- (c) 2010 Jiri Appl <jiri@appl.name>
#include <iostream>

#include <wibble/commandline/parser.h>
#include <wibble/sys/fs.h>

#include <divine/config.h>
#include <divine/generator/compact.h>
#include <divine/porcp.h>
#include <divine/algorithm/probabilistic.h>
#include <divine/report.h>

using namespace divine;
using namespace wibble;
using namespace commandline;

struct Main {
    Config config;

    BoolOption *o_report, *o_onlyQualitative, *o_iterativeOptimization;
    IntOption *o_workers;

    int argc;
    const char **argv;
    commandline::StandardParserWithManpage opts;

    Main( int _argc, const char **_argv )
        : argc( _argc ), argv( _argv ),
          opts( "DiVinE Probabilistic", "2.0", 1, "DiVinE Team <divine@fi.muni.cz>" )
    {
        setupCommandline();
        parseCommandline();

        Report report( config );
        Result res;

        report.algorithm = "Probabilistic";
        report.generator = "Compact";

        try {
            algorithm::Probabilistic< algorithm::NonPORGraph< generator::Compact > > alg( &config );
            alg.domain().mpi.init();
            alg.init( &alg.domain() );
            alg.domain().mpi.start();
            report.mpiInfo( alg.domain().mpi );

            res = alg.run();
        } catch ( std::exception &e ) {
            die( std::string( "FATAL: " ) + e.what() );
        } catch ( const char* s ) {
            die( std::string( "FATAL: " ) + s );
        }

        report.finished( res );
        if ( o_report->boolValue() ) report.final( std::cout );
    }

    static void die( std::string bla ) __attribute__((noreturn))
    {
        std::cerr << bla << std::endl;
        exit( 1 );
    }

    void setupCommandline()
    {
        opts.usage = "<command> [command-specific options] <input file>";
        opts.description = "A Distributed Quantitative Model Checking tool for Probabilistic Models";

        o_report = opts.add< BoolOption >(
            "report", 'r', "report", "", "output standardised report" );

        o_workers = opts.add< IntOption >(
            "workers", 'w', "workers", "",
            "number of worker threads (default: 2)" );
        o_workers->setValue( 2 );

        o_onlyQualitative = opts.add< BoolOption >(
            "qualitative", 'q', "qualitative", "",
            "only qualitative analysis (default: quantitative)" );

        o_iterativeOptimization = opts.add< BoolOption >(
            "iterative", 'i', "iterative", "",
            "iteratively compute probabilities of SCCs to reduce lp_solve computation time" );
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

        config.workers = o_workers->intValue();
        config.input = input;
        config.onlyQualitative = o_onlyQualitative->boolValue();
        config.iterativeOptimization = o_iterativeOptimization->boolValue();

        if ( access( input.c_str(), R_OK ) )
            die( "FATAL: cannot open input file " + input + " for reading" );
    }
};

int main( int argc, const char **argv )
{
    Main m( argc, argv );
    return 0;
}
