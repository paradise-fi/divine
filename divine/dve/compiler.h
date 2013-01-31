// -*- C++ -*- (c) 2009, 2010 Milan Ceska & Petr Rockai
//*
#include <string>
#include <fstream>
#include <math.h>
#include <map>
#include <vector>
#include <string>
#include <wibble/string.h>
#include <divine/dve/parse.h>
#include <divine/dve/interpreter.h>

#ifndef TOOLS_DVECOMPILE_H
#define TOOLS_DVECOMPILE_H

using namespace std;

namespace divine {
namespace dve {

namespace compiler {

struct ExtTransition
{
    int synchronized;
    parse::Transition *first;
    parse::Transition *second; //only when first transition is synchronized;
    parse::Transition *property; // transition of property automaton
};

struct DveCompiler
{
    bool many;
    int current_label;

    parse::System * ast;
    System * system;

    bool have_property;
    map< std::string, map< std::string, vector< ExtTransition > > > transition_map;
    map< std::string, vector< parse::Transition* > > channel_map;

    vector< parse::Transition* > property_transitions;
    vector< parse::Transition* >::iterator iter_property_transitions;

    map< std::string, vector< parse::Transition* > >::iterator iter_channel_map;
    vector< parse::Transition* >::iterator iter_transition_vector;
    map< std::string, vector< ExtTransition > >::iterator iter_process_transition_map;
    map< std::string, map< std::string, vector< ExtTransition > > >::iterator iter_transition_map;
    vector< ExtTransition >::iterator iter_ext_transition_vector;

    string m_line;
    ostream *m_output;
    int m_indent;
    bool in_process, process_empty;

    void read( std::string path ) {
        std::ifstream file;
        file.open( path.c_str() );
        dve::IOStream stream( file );
        dve::Lexer< dve::IOStream > lexer( stream );
        dve::Parser::Context ctx( lexer );
        try {
            ast = new parse::System( ctx );
            //system = new System( ast );
        } catch (...) {
            ctx.errors( std::cerr );
            throw;
        }
    }

    void indent() { ++m_indent; }
    void deindent() { --m_indent; }

    void append( std::string l ) { m_line += l; }

    void outline() {
        ostream &out = *m_output;
        for ( int i = 0; i < m_indent; ++i )
            out << "    ";
        out << m_line << std::endl;
        m_line = "";
    }

    void line( std::string l = "" ) {
        append( l );
        outline();
    }

    void start_process();
    void ensure_process();
    void end_process( std::string name );

    DveCompiler()
        : current_label(1), m_indent( 0 ),
          in_process( false ), process_empty( true )
    {}

    bool chanIsBuffered( parse::Identifier chan );

    void analyse_transition( parse::Transition * t,
                             vector< ExtTransition > &ext_transition_vector );
    void analyse();

    void write_C( parse::Expression & expr, std::ostream & ostr, std::string state_name, std::string context );

    void write_C( parse::LValue & expr, std::ostream & ostr, std::string state_name, std::string context );
    void write_C( parse::RValue & expr, std::ostream & ostr, std::string state_name, std::string context );

    bool m_if_disjoint;
    bool m_if_empty;

    void if_begin( bool disjoint ) {
        m_if_empty = true;
        m_if_disjoint = disjoint;
        append( "if ( " );
    }

    void if_clause( std::string c ) {
        if ( !m_if_empty ) {
            if ( m_if_disjoint )
                append( " || " );
            else
                append( " && " );
        }
        m_if_empty = false;
        append( " ( " );
        append( c );
        append( " ) " );
    }

    void if_cexpr_clause( parse::Expression *expr, std::string state, std::string context ) {
        if (!expr)
            return;
        if_clause( cexpr( *expr, state, context ) );
    }

    void if_end() {
        if ( m_if_empty ) {
            if ( m_if_disjoint )
                append( "false " );
            else
                append( "true " );
        }
        append( ")" );
        outline();
    }

    void assign( std::string left, std::string right ) {
        line( left + " = " + right + ";" );
    }

    void declare( std::string type, std::string name, int array = 0 ) {
        line( type + " " + name +
              ( array ? ("[" + wibble::str::fmt( array ) + "]") : "" ) +
              ";" );
    }

    std::string relate( std::string left, std::string op, std::string right ) {
        return left + " " + op + " " + right;
    }

    std::string process_state( parse::Process &p, std::string state ) {
        return state + "._control." + p.name.name();
    }

    std::string channel_items( std::string chan, std::string state ) {
        return state + "." + chan + ".number_of_items";
    }

    std::string channel_item_at( std::string chan, std::string pos, int x, std::string state ) {
        return state + "." + chan + ".content[" + pos + "].x" + wibble::str::fmt( x );
    }

    parse::ChannelDeclaration & getChannel( std::string chan ) {
        for ( parse::ChannelDeclaration &c : ast->chandecls ) {
            if ( c.name == chan )
                return c;
        }
    }

    std::string getVariable( std::string name, std::string context ) {
        parse::Process &p = getProcess( context );
        for ( parse::Declaration &decl : p.decls ) {
            if ( decl.name == name )
                return context + "." + name;
        }
        return name;
    }

    int channel_capacity( std::string chan ) {
        return getChannel( chan ).size;
    }

    void transition_guard( ExtTransition * et, std::string in );
    void transition_effect( ExtTransition * et, std::string in, std::string out );

    bool is_property( parse::Process &p ) {
        return p.name.name() == ast->property.name();
    }

    std::string cexpr( parse::Expression &expr, std::string state, std::string context );
    std::string cexpr( parse::LValue &expr, std::string state, std::string context );
    void print_cexpr( parse::Expression &expr, std::string state, std::string context )
    {
        line( cexpr( expr, state, context ) + ";" );
    }

    void new_label() {
        if (many)
            return;
        append( std::string( "l" ) + wibble::str::fmt( current_label ) + ": " );
        current_label ++;
    }

    void block_begin() { line( "{" ); indent(); }
    void block_end() { deindent(); line( "}" ); }

    std::string in_state( parse::Process process, int state, std::string from_state ) {
        return "(" + process_state( process, from_state ) + " == " + wibble::str::fmt( state ) + ")";
    }

    void setOutput( std::ostream &o ) {
        m_output = &o;
    }

    bool isAccepting( parse::Process &p, int stateID ) {
        for ( parse::Identifier &accept : p.accepts ) {
            if ( accept.name() == p.states[ stateID ].name() )
                return true;
        }
        return false;
    }

    bool isCommited( parse::Process &p, int stateID ) {
        for ( parse::Identifier &commit : p.commits ) {
            if ( commit.name() == p.states[ stateID ].name() )
                return true;
        }
        return false;
    }

    bool isCommited( parse::Process &p, std::string state ) {
        for ( parse::Identifier &commit : p.commits ) {
            if ( commit.name() == state )
                return true;
        }
        return false;
    }

    int getStateId( parse::Process &p, std::string state ) {
        for ( int i = 0; i < p.states.size(); i++ ) {
            if ( p.states[ i ].name() == state )
                return i;
        }
        return -1;
    }

    parse::Process & getProcess( std::string proc ) {
        for ( parse::Process &p : ast->processes )
            if ( p.name.name() == proc )
                return p;
    }

    void yield_state();
    void new_output_state();

    void gen_constants();
    void gen_successors();
    void gen_is_accepting();
    void gen_header();
    void gen_state_struct();
    void gen_initial_state();

    void print_generator();
};

}

}
}
#endif
