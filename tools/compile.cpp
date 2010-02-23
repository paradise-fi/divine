#include "compile.h"
#include <wibble/string.h>
#include <wibble/sys/fs.h>

#ifdef HAVE_MURPHI
bool mucompile( const char *, const char * );

namespace divine {

using namespace wibble;

void Compile::compileMurphi( std::string in ) {
    std::string outfile = str::basename( in ) + ".cpp";
    if (!mucompile( in.c_str(), outfile.c_str() ))
        die( "FATAL: Error in murphi compilation. Please consult above messages." );

    std::string mu = sys::fs::readFile( outfile );
    sys::fs::deleteIfExists( outfile );

    sys::fs::writeFile( outfile, "\
" + mu + "\
\n\
extern \"C\" int get_state_size() {\n\
    args = new argclass( 0, NULL ); // FIXME\n\
    std::cerr << \"symmetry: \" << args->symmetry_reduction.value << \", \"\n\
              << args->multiset_reduction.value << \", \"\n\
              << args->sym_alg.mode << std::endl;\n\
    MuGlobal::init();\n\
    return sizeof( state );\n\
}\n\
\n\
extern \"C\" void get_initial_state( char *to ) {\n\
    MuGlobal::get().startgen->Code( 0 );\n\
    StateCopy( (state *)to, workingstate );\n\
}\n\
\n\
extern \"C\" int get_successor( int h, char *from, char *to ) {\n\
    unsigned rule = h - 1;\n\
    StateCopy( workingstate, (state *) from );\n\
  another:\n\
    MuGlobal::get().nextgen->SetNextEnabledRule( rule );\n\
    if ( rule >= numrules )\n\
        return 0;\n\
    MuGlobal::get().nextgen->Code( rule );\n\
    if ( StateCmp( workingstate, (state *) from) == 0 ) {\n\
        ++ rule;\n\
        goto another;\n\
    }\n\
    MuGlobal::get().symmetry->Normalize( workingstate ); // workingstate->Normalize();\n\
    StateCopy( (state *)to, workingstate );\n\
    return rule + 2;\n\
}\n" );

    gplusplus( outfile, str::basename( in ) + ".so", std::string( "-Wno-write-strings" ) );
}

}
#else

namespace divine {

void Compile::compileMurphi( std::string ) {
    die( "FATAL: This binary does not support compiling murphi models." );
}

}
#endif
