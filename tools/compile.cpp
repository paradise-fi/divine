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
" + mu + compile_defines_str + pool_h_str + blob_h_str + generator_custom_api_h_str + "\
\n\
using namespace divine;\n\
extern \"C\" void setup( CustomSetup *s ) {\n\
    if ( !MuGlobal::init_once() ) return;\n\
    args = new argclass( 0, NULL ); // FIXME\n\
    std::cerr << \"symmetry: \" << args->symmetry_reduction.value << \", \"\n\
              << args->multiset_reduction.value << \", \"\n\
              << args->sym_alg.mode << std::endl;\n\
}\n\
\n\
extern \"C\" void get_initial( CustomSetup *setup, Blob *to ) {\n\
    MuGlobal::get().startgen->Code( 0 );\n\
    Blob b_out( *(setup->pool), setup->slack + sizeof( state ) );\n\
    b_out.clear( 0, setup->slack );\n\
    *to = b_out;\n\
    StateCopy( &b_out.get< state >( setup->slack ), workingstate );\n\
}\n\
\n\
extern \"C\" int get_successor( CustomSetup *setup, int h, Blob from, Blob *to ) {\n\
    unsigned rule = h - 1;\n\
    StateCopy( workingstate, &from.get< state >( setup->slack ) );\n\
  another:\n\
    MuGlobal::get().nextgen->SetNextEnabledRule( rule );\n\
    if ( rule >= numrules )\n\
        return 0;\n\
    MuGlobal::get().nextgen->Code( rule );\n\
    if ( StateCmp( workingstate, &from.get< state >( setup->slack ) ) == 0 ) {\n\
        ++ rule;\n\
        goto another;\n\
    }\n\
    MuGlobal::get().symmetry->Normalize( workingstate ); // workingstate->Normalize();\n\
    \n\
    Blob b_out( *(setup->pool), setup->slack + sizeof( state ) );\n\
    b_out.clear( 0, setup->slack );\n\
    *to = b_out;\n\
    StateCopy( &b_out.get< state >( setup->slack ), workingstate );\n\
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
