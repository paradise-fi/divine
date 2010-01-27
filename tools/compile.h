// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <wibble/commandline/parser.h>
#include <wibble/string.h>
#include "dvecompile.h"

struct Compile {
    Engine *cmd_compile;
    commandline::StandardParserWithMandatoryCommand &opts;

    void die( std::string bla ) __attribute__((noreturn))
    {
        std::cerr << bla << std::endl;
        exit( 1 );
    }

    void compileDve( std::string in ) {
        dve_compiler compiler;
        compiler.read( in.c_str() );
        compiler.analyse();

        std::string outfile = str::basename( in ) + ".c";
        std::ofstream out( outfile.c_str() );
        compiler.setOutput( out );

        compiler.print_header();
        compiler.print_state_struct();
        compiler.print_initial_state();
        compiler.print_generator();

        std::stringstream cmd;
        cmd << "g++ -O2 -shared -fPIC -o " << str::basename( in ) + ".so" << " " << outfile;
        system( cmd.str().c_str() );
    }

    void main() {
        std::string input = opts.next();
        if ( access( input.c_str(), R_OK ) )
            die( "FATAL: cannot open input file " + input + " for reading" );
        if ( str::endsWith( input, ".dve" ) ) {
            compileDve( input );
        } else {
            std::cerr << "Do not know how to compile this file type." << std::endl;
        }
    }

    Compile( commandline::StandardParserWithMandatoryCommand &_opts )
        : opts( _opts)
    {
        cmd_compile = _opts.addEngine( "compile",
                                       "<input>",
                                       "model compiler");
    }

};
