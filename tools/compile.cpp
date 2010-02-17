#include "compile.h"
#include <wibble/string.h>

namespace divine {

using namespace wibble;

void Compile::compileMurphi( std::string in ) {
    std::string outfile = std::string( in, 0, in.length() - 2 ) + ".C"; // ick
    run( "mu " + in );

    ofstream c( outfile.c_str(), ios_base::app );
    c << "\
StartStateGenerator startgen; // FIXME\n\
NextStateGenerator nextgen; // FIXME\n\
\n\
extern \"C\" int get_state_size() {\n\
    args = new argclass( 0, NULL ); // FIXME\n\
    theworld.to_state(NULL); // trick : marks variables in world\n\
    std::cerr << \"symmetry: \" << args->symmetry_reduction.value << \", \"\n\
              << args->multiset_reduction.value << \", \"\n\
              << args->sym_alg.mode << std::endl;\n\
    return sizeof( state );\n\
}\n\
\n\
extern \"C\" void get_initial_state( char *to ) {\n\
    startgen.Code( 0 );\n\
    StateCopy( (state *)to, workingstate );\n\
}\n\
\n\
extern \"C\" int get_successor( int h, char *from, char *to ) {\n\
    unsigned rule = h - 1;\n\
    StateCopy( workingstate, (state *) from );\n\
  another:\n\
    nextgen.SetNextEnabledRule( rule );\n\
    if ( rule >= numrules )\n\
        return 0;\n\
    nextgen.Code( rule );\n\
    if ( StateCmp( workingstate, (state *) from) == 0 ) {\n\
        ++ rule;\n\
        goto another;\n\
    }\n\
    workingstate->Normalize();\n\
    StateCopy( (state *)to, workingstate );\n\
    return rule + 2;\n\
}\n" << std::endl;
    c.close();

    gplusplus( outfile, str::basename( in ) + ".so",
               std::string( "-Wno-write-strings -I" ) + getenv( "MU_INCLUDE_PATH" ) );
}

}
