// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <wibble/commandline/parser.h>
#include <wibble/string.h>
#include "dvecompile.h"
#include <wibble/sys/fs.h>

namespace divine {

extern const char *generator_custom_api_h_str;
extern const char *pool_h_str;
extern const char *circular_h_str;
extern const char *blob_h_str;
extern const char *compile_defines_str;

using namespace wibble;

struct Compile {
    commandline::Engine *cmd_compile;
    commandline::StandardParserWithMandatoryCommand &opts;

    void die( std::string bla ) __attribute__((noreturn))
    {
        std::cerr << bla << std::endl;
        exit( 1 );
    }

    void run( std::string command ) {
        int status = system( command.c_str() );
#ifdef POSIX
        if ( status != -1 && WEXITSTATUS( status ) != 0 )
            die( "Error running external command: " + command );
#endif
    }

    void gplusplus( std::string in, std::string out, std::string flags = "" ) {
        std::stringstream cmd;
        std::string multiarch =
#if defined(USE_GCC_M32)
            "-m32 "
#elif defined(USE_GCC_M64)
            "-m64 "
#else
            ""
#endif
            ;
        cmd << "g++ -O2 -shared -fPIC " << multiarch << flags << " -o " << out << " " << in;
        run( cmd.str() );
    }

    void compileDve( std::string in ) {
        dve_compiler compiler;
        compiler.read( in.c_str() );
        compiler.analyse();

        std::string outfile = str::basename( in ) + ".cpp";
        std::ofstream out( outfile.c_str() );
        compiler.setOutput( out );
        compiler.print_generator();

        gplusplus( outfile, str::basename( in ) + ".so" );
    }

    void compileMurphi( std::string in );

    void main() {
        std::string input = opts.next();
        if ( access( input.c_str(), R_OK ) )
            die( "FATAL: cannot open input file " + input + " for reading" );
        if ( str::endsWith( input, ".dve" ) )
            compileDve( input );
        else if ( str::endsWith( input, ".m" ) )
            compileMurphi( input );
        else {
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

}
