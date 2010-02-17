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
        compiler.print_generator();

        std::stringstream cmd;
        cmd << "g++ -O2 -shared -fPIC -o " << str::basename( in ) + ".so" << " " << outfile;
        int status = system( cmd.str().c_str() );
#ifdef POSIX
        if ( status != -1 && WEXITSTATUS( status ) != 0 )
            die( "Error compiling intermediate C++ code." );
#endif
    }

    void compileMurphi( std::string in ) {
        std::stringstream cmd;
        cmd << "mu " + in;
        std::string outfile = std::string( in, 0, in.length() - 2 ) + ".C"; // ick
        int status = system( cmd.str().c_str() ); // ought to leave basename( in ).C around
        if ( status != -1 && WEXITSTATUS( status ) != 0 )
            die( "Error running the mu compiler." );
        cmd.str( "" );
        ofstream c( outfile.c_str(), ios_base::app );
        c << "StartStateGenerator startgen; // FIXME" << std::endl;
        c << "NextStateGenerator nextgen; // FIXME" << std::endl;
        c << "extern \"C\" int get_state_size() { \n\
                   args = new argclass( 0, NULL ); // FIXME \n\
                   theworld.to_state(NULL); // trick : marks variables in world \n\
                   std::cerr << \"symmetry: \" << args->symmetry_reduction.value << \", \" \n\
                             << args->multiset_reduction.value << \", \" \n     \
                             << args->sym_alg.mode << std::endl; \n\
                   return sizeof( state ); \n\
              }" << std::endl;
        c << "extern \"C\" void get_initial_state( char *to ) {\n\
                   startgen.Code( 0 ); \n\
                   StateCopy( (state *)to, workingstate ); \n\
              }" << std::endl;
        c << "extern \"C\" int get_successor( int h, char *from, char *to ) { \n\
                   unsigned rule = h - 1; \n\
                   StateCopy( workingstate, (state *) from ); \n\
                 another: \n\
                   nextgen.SetNextEnabledRule( rule ); \n\
                   if ( rule >= numrules ) \n\
                          return 0; \n\
                   nextgen.Code( rule ); \n\
                   if ( StateCmp( workingstate, (state *) from) == 0 ) { \n\
                          ++ rule; \n\
                          goto another; \n\
                   } \n\
                   workingstate->Normalize(); \n\
                   StateCopy( (state *)to, workingstate ); \n\
                   return rule + 2; \n\
        }" << std::endl;
        c.close();
        cmd << "g++ -Wno-write-strings -I" << getenv( "MU_INCLUDE_PATH" )
            << " -O2 -shared -fPIC -o " << str::basename( in ) + ".so" << " " << outfile;
        status = system( cmd.str().c_str() );
        if ( status != -1 && WEXITSTATUS( status ) != 0 )
            die( "Error compiling intermediate C++ code." );
    }

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
