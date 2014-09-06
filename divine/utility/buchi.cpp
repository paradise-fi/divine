#include "buchi.h"
#include <wibble/regexp.h>

#if OPT_LTL3BA
#include <external/ltl3ba/ltl3ba.h>
#endif

using namespace divine;

#if OPT_LTL3BA
// ltl3ba does not have the proper interface
void reinit();
int tl_main( std::string &argv );
extern int tl_det_m;
extern int tl_sim_r;
extern int tl_print;

std::vector< Buchi::DNFClause > Buchi::gclause;

int Buchi::ltl3ba_parse( std::string ltl ) {
    LTL_parse_t parse;
    LTL_formul_t F;
    parse.nacti( ltl );
    if ( !parse.syntax_check( F ) ) {
        std::cerr << "Error: Syntax error in LTL formula: '" << ltl << "'." << std::endl;
        return -1;
    }
    F = F.negace();
    std::stringstream strF;
    strF << F;
    ltl = strF.str();

    // run ltl3ba
    reinit();
    tl_det_m = 1;   // try to determinize
    tl_sim_r = 1;   // simulation reduction
    tl_print = 0;   // do not print anything
    return tl_main( ltl );  // returns zero if successful
}

void Buchi::ltl3ba_finalize() {
    // Normally, bdd_done() would be called here, but that causes heap corruption in the bdd library if it is used again
}

BState *Buchi::nodeBegin() {
    return get_buchi_states()->prv;
}

BState *Buchi::nodeEnd() {
    return get_buchi_states();
}

bool Buchi::isAccepting( BState* s ) {
    return s->final == get_buchi_accept();
}

bool Buchi::isInitial( BState* s ) {
    return s->id == -1;
}

void Buchi::next( BState*& s ) {
    s = s->prv;
}

int Buchi::getId( BState* s ) {
    return s->incoming;
}

void Buchi::assignId( BState* s, int id ) {
    s->incoming = id;
}

int Buchi::getEdgeCount( BState* s ) {
    return s->trans->size();
}

int Buchi::parseEdge( BState* s, int i ) {
    auto iter = s->trans->begin();
    std::advance( iter, i );
    gclause.clear();
    if ( iter->second == bdd_false() ); // impossible edge
    else if ( iter->second == bdd_true() )  // always enabled
        gclause.push_back( DNFClause() );
    else    // one or more conditional edges
        bdd_allsat( iter->second, allsatHandler );
    return getId( iter->first );
}

void Buchi::allsatHandler( char* varset, int size ) {
    gclause.push_back( DNFClause() );
    for ( int v = 0; v < size; v++ ) {
        if ( varset[v] < 0 ) continue;
        // varset[v] == 0 means negation
        gclause.back().push_back( std::make_pair( varset[v] > 0, get_buchi_symbol( v ) ) );
    }
}
#endif

void Buchi::buildEmpty() {
    nodes.resize( 1 );
    nodes[ 0 ].isAcc = false;
    nodes[ 0 ].tr.resize( 1 );
    nodes[ 0 ].tr[ 0 ] = std::make_pair( 0, 0 );
    initId = 0;
}

bool Buchi::readLTLFile( const std::string& fname, std::vector< std::string >& props, std::map< std::string, std::string >& defs ) {
    std::fstream file( fname, std::fstream::in );
    if ( !file.is_open() ) return false;
    wibble::ERegexp def( "^[ \t]*#define ([^ \t]+)[ \t]+(.*)", 3 );
    wibble::ERegexp prop( "^[ \t]*#property (.*)", 2 );
    while ( file.good() ) {
        std::string line;
        getline( file, line );
        if ( def.match( line ) )
            defs[ def[1] ] = def[2];
        if ( prop.match( line ) )
            props.push_back( prop[1] );
    }
    return !props.empty();
}

std::ostream& Buchi::printAutomaton( std::ostream& o, const std::string& property, const std::map< std::string, std::string >& defs ) {
    Buchi buchi;
    std::vector< std::string > guards ( 1 );
    buchi.build( property, [ &guards, &defs ]( const Buchi::DNFClause& clause ) -> int {
        if ( clause.empty() )
            return 0;
        guards.push_back( std::string() );
        for ( auto lit = clause.begin(); lit != clause.end(); ++lit ) {
            auto found = defs.find( lit->second );
            const std::string& strLit = ( found == defs.end() ) ? lit->second : found->second;  // substitute definitions
            if ( !lit->first )
                guards.back() += "!";
            guards.back() += strLit;
            if ( lit + 1 != clause.end() )
                guards.back() += " && ";
        }

        return guards.size() - 1;
    });

    o << "\tstates q0";
    for ( unsigned int i = 1; i < buchi.size(); ++i )
        o << ", q" << i;
    o << ";\n\tinit q" << buchi.getInitial();
    o << ";\n\taccepting ";
    bool first = true;
    for ( unsigned int i = 0; i < buchi.size(); ++i ) {
        if ( buchi.isAccepting( i ) ) {
            if ( first )
                o << "q";
            else
                o << ", q";
            o << i;
            first = false;
        }
    }
    o << ";\n\ttrans\n";
    first = true;
    for ( unsigned int i = 0; i < buchi.size(); ++i ) {
        for ( auto tr = buchi.transitions( i ).begin(); tr != buchi.transitions( i ).end(); ++tr ) {
            if ( !first ) o << ",\n";
            first = false;
            o << "\t\tq" << i << " -> q" << tr->first;
            if ( tr->second )
                o << " {guard " << guards[ tr->second ] << "}";
            else
                o << " {}";
        }
    }
    o << ";\n";
    return o;
}
