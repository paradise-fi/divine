#include <wibble/string.h>
#include <wibble/sys/fs.h>

#include "compile.h"
#include "combine.h"
#include <divine/utility/buchi.h>

bool mucompile( const char *, const char * );

namespace divine {

using namespace wibble;

std::string ltl_to_c( int id, std::string ltl )
{
    std::stringstream s, decls;
    Buchi buchi;
    std::vector< std::string > guards ( 1 ); /* guards[0] is a dummy element */
    std::set< std::string > symbols;
    buchi.build( ltl, [ &guards, &symbols ]( const Buchi::DNFClause &clause ) -> int {
        if ( clause.empty() )
            return 0;
        guards.emplace_back();
        for ( auto lit = clause.begin(); lit != clause.end(); ++lit ) {
            if ( lit != clause.begin() )
                guards.back() += " && ";
            if ( !lit->first )
                guards.back() += "!";
            guards.back() += "prop_";
            guards.back() += lit->second;
            guards.back() += "( &sys_setup, n )";
            symbols.insert( lit->second );
        }

        return guards.size() - 1;
    });

    /* the states are numbered 0, ..., buchi.size() - 1; however, CESMI uses 0
     * for special purpose (transition not applicable), so we add 1 to the
     * state numbers everywhere */

    s << "int buchi_initial_" << id << " = " << buchi.getInitial() + 1 << ";\n\n";

    s << "int buchi_accepting_" << id << "( int id ) {" << std::endl;
    for ( unsigned i = 0; i < buchi.size(); ++i ) {
        if ( buchi.isAccepting( i ) )
            s << "    if ( id == " << i + 1 << " ) return true;" << std::endl;
    }
    s << "    return false;" << std::endl;
    s << "}" << std::endl;
    s << std::endl;

    s << "int buchi_next_" << id
      << "( int from, int transition, cesmi_setup *setup, cesmi_node n ) {"
      << std::endl
      << "    cesmi_setup sys_setup = system_setup( setup );"
      << std::endl;
    int buchi_next = 0;
    for ( unsigned i = 0; i < buchi.size(); ++i ) {
        for ( const auto & tr : buchi.transitions( i ) ) {
            s << "    if ( transition == " << buchi_next << " ) {" << std::endl;
            s << "        if ( from == " << i + 1;
            if ( tr.second ) {
                s << " && ( " << guards[ tr.second ] << " ) ";
            }
            s << " )" << std::endl;
            s << "             return " << tr.first + 1 << ";" << std::endl;
            s << "        return 0;" << std::endl;
            s << "    }" << std::endl;
            ++ buchi_next;
        }
    }

    s << "    return -1;" << std::endl;
    s << "}" << std::endl;

    for ( const auto & s : symbols )
        decls << "extern \"C\" bool prop_" << s << "( cesmi_setup *setup, cesmi_node n );" << std::endl;

    decls << "cesmi_setup system_setup( const cesmi_setup *setup );" << std::endl;

    return decls.str() + "\n" + s.str();
}

#ifdef O_MURPHI
void Compile::compileMurphi( std::string in ) {
    std::string outfile = str::basename( in ) + ".cpp";
    if (!mucompile( in.c_str(), outfile.c_str() ))
        die( "FATAL: Error in murphi compilation. Please consult above messages." );

    std::string mu = sys::fs::readFile( outfile );
    sys::fs::deleteIfExists( outfile );

    sys::fs::writeFile( outfile, "\
" + mu + compile_defines_str + toolkit_pool_h_str + toolkit_blob_h_str + generator_cesmi_client_h_str + "\
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

    runCompiler( outfile, str::basename( in ) + ".so", std::string( "-Wno-write-strings" ) );
}
#else

void Compile::compileMurphi( std::string ) {
    die( "FATAL: This binary does not support compiling murphi models." );
}

#endif

}
