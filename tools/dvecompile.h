// -*- C++ -*- (c) 2009, 2010 Milan Ceska & Petr Rockai
#include <divine/legacy/system/dve/dve_explicit_system.hh>
#include <string>
#include <math.h>
#include <map>
#include <vector>
#include <wibble/string.h>

using namespace divine;
using namespace std;

struct ext_transition_t
{
    int synchronized;
    dve_transition_t *first;
    dve_transition_t *second; //only when first transition is synchronized;
    dve_transition_t *property; // transition of property automaton
};

struct dve_compiler: public dve_explicit_system_t
{
    bool many;
    int current_label;

    bool have_property;
    map<size_int_t,map<size_int_t,vector<ext_transition_t> > > transition_map;
    map<size_int_t,vector<dve_transition_t*> > channel_map;

    vector<dve_transition_t*> property_transitions;
    vector<dve_transition_t*>::iterator iter_property_transitions;

    map<size_int_t,vector<dve_transition_t*> >::iterator iter_channel_map;
    vector<dve_transition_t*>::iterator iter_transition_vector;
    map<size_int_t,vector<ext_transition_t> >::iterator iter_process_transition_map;
    map<size_int_t,map<size_int_t,vector<ext_transition_t> > >::iterator iter_transition_map;
    vector<ext_transition_t>::iterator iter_ext_transition_vector;

    string m_line;
    ostream *m_output;
    int m_indent;

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

    dve_compiler(error_vector_t & evect=gerr)
        : explicit_system_t(evect), dve_explicit_system_t(evect), current_label(0), m_indent( 0 )
    {}
    virtual ~dve_compiler() {}

    void analyse_transition( dve_transition_t * transition,
                             vector<ext_transition_t> &ext_transition_vector );
    void analyse();

    void write_C(dve_expression_t & expr, std::ostream & ostr, std::string state_name);

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

    void if_cexpr_clause( dve_expression_t *expr, std::string state ) {
        if (!expr)
            return;
        if_clause( cexpr( *expr, state ) );
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

    std::string relate( std::string left, std::string op, std::string right ) {
        return left + " " + op + " " + right;
    }

    std::string process_name( int i ) {
        return get_symbol_table()->get_process( i )->get_name();
    }

    std::string channel_name( int i ) {
        return get_symbol_table()->get_channel( i )->get_name();
    }

    std::string process_state( int i, std::string state ) {
        return state + "." + process_name( i ) + ".state";
    }

    std::string channel_items( int i, std::string state ) {
        return state + "." + channel_name( i ) + ".number_of_items";
    }

    std::string channel_item_at( int i, std::string pos, int x, std::string state ) {
        return state + "." + channel_name( i ) + ".content[" + pos + "].x" + wibble::str::fmt( x );
    }

    int channel_capacity( int i ) {
        return get_symbol_table()->get_channel( i )->get_channel_buffer_size();
    }

    void transition_guard( ext_transition_t *, std::string );
    void transition_effect( ext_transition_t *, std::string, std::string );

    bool is_property( int i ) {
        return get_with_property() && i == get_property_gid();
    }

    std::string cexpr( dve_expression_t &expr, std::string state );
    void print_cexpr( dve_expression_t &expr, std::string state )
    {
        line( cexpr( expr, state ) + ";" );
    }

    void new_label() {
        if (many)
            return;
        append( std::string( "l" ) + wibble::str::fmt( current_label ) + ": " );
        current_label ++;
    }

    void block_begin() { line( "{" ); indent(); }
    void block_end() { deindent(); line( "}" ); }

    std::string in_state( int process, int state, std::string from_state ) {
        return process_state( process, from_state ) + " == " + wibble::str::fmt( state );
    }

    void setOutput( std::ostream &o ) {
        m_output = &o;
    }

    void yield_state();
    void new_output_state();

    void gen_successors();
    void gen_is_accepting();
    void gen_header();
    void gen_state_struct();
    void gen_initial_state();

    void print_generator();
};
