#include <wibble/string.h>
#include <wibble/sys/fs.h>

#include "compile.h"
#include "combine.h"

bool mucompile( const char *, const char * );

namespace divine {

using namespace wibble;

#ifdef O_LTL3BA
void print_buchi_trans_label(bdd label, std::ostream& out) {
    std::stringstream s;
	  while (label != bddfalse) {
        bdd current = bdd_satone(label);
        label -= current;

        out << "(";
        while (current != bddtrue && current != bddfalse) {
            bdd high = bdd_high(current);
            if (high == bddfalse) {
                out << "!prop_" << get_buchi_symbol(bdd_var(current)) << "( setup, n )";
                current = bdd_low(current);
            } else {
                out << "prop_" << get_buchi_symbol(bdd_var(current)) << "( setup, n )";;
                current = high;
            }
            if (current != bddtrue && current != bddfalse) out << " && ";
        }
        out << ")";
        if (label != bddfalse) out << " || ";
    }
}

std::string buchi_to_c( int id, BState* bstates, int accept, std::list< std::string > symbols )
{
    std::stringstream s, decls;
    typedef std::map<BState*, bdd> TransMap;

    BState *n;
    int nodes = 1;

    s << "int buchi_accepting_" << id << "( int id ) {" << std::endl;
    for ( n = bstates->prv; n != bstates; n = n->prv ) {
        n->incoming = nodes++;
        if ( n->final == accept )
            s << "    if ( id == " << n->incoming << " ) return true;" << std::endl;
    }
    s << "    return false;" << std::endl;
    s << "}" << std::endl;
    s << std::endl;

    s << "int buchi_next_" << id
      << "( int from, int transition, cesmi_setup *setup, cesmi_node n ) {"
      << std::endl;
    int buchi_next = 0;
    for ( n = bstates->prv; n != bstates; n = n->prv ) {
        for ( TransMap::iterator t = n->trans->begin(); t != n->trans->end(); ++t ) {
            s << "    if ( transition == " << buchi_next << " ) {" << std::endl;
            s << "        if ( from == " << n->incoming;
            if ( t->second != bdd_true() ) {
                s << " && ";
                print_buchi_trans_label(t->second, s);
            }
            s << " )" << std::endl;
            s << "             return " << t->first->incoming << ";" << std::endl;
            s << "        return 0;" << std::endl;
            s << "    }" << std::endl;
            ++ buchi_next;
        }
    }

    s << "    return -1;" << std::endl;
    s << "}" << std::endl;

    for ( std::list< std::string >::iterator i = symbols.begin(); i != symbols.end(); ++ i )
        decls << "extern \"C\" bool prop_" << *i << "( cesmi_setup *setup, cesmi_node n );" << std::endl;

    return decls.str() + "\n" + s.str();
}

std::string ltl_to_c( int id, std::string ltl )
{
    Combine::ltl3baTranslation( ltl );
    return buchi_to_c( id, get_buchi_states(), get_buchi_accept(), get_buchi_all_symbols() );
}

#else

std::string ltl_to_c( int id, std::string ltl )
{
    assert_unimplemented();
}

#endif

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
