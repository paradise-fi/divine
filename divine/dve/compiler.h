// -*- C++ -*- (c) 2009, 2010 Milan Ceska & Petr Rockai
//             (c) 2013 Jan Kriho

#include <string>
#include <fstream>
#include <cmath>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <brick-string.h>
#include <divine/dve/preprocessor.h>
#include <divine/dve/interpreter.h>

#ifndef TOOLS_DVECOMPILE_H
#define TOOLS_DVECOMPILE_H

namespace divine {
namespace dve {

namespace compiler {

struct ExtTransition
{
    int synchronized;
    parse::Transition *first;
    parse::Transition *second; //only when first transition is synchronized;
    parse::Transition *property; // transition of property automaton

    ExtTransition() : synchronized( 0 ), first( 0 ), second( 0 ), property( 0 ) {}
};

struct DveCompiler
{
    bool many;
    int current_label;

    parse::System * ast;
    System * system;

    bool have_property;
    std::map< std::string, std::map< std::string, std::vector< std::vector< ExtTransition > > > > transition_map;
    std::map< std::string, std::vector< parse::Transition* > > channel_map;
    std::map< std::string, std::map< std::string, std::vector< parse::Transition * > > > procChanMap;

    std::vector< std::vector< parse::Transition* > > property_transitions;
    std::vector< parse::Transition* >::iterator iter_property_transitions;

    std::map< std::string, std::vector< parse::Transition* > >::iterator iter_channel_map;
    std::map< std::string, std::map< std::string, std::vector< parse::Transition * > > >::iterator iter_procChanMap;
    std::vector< parse::Transition* >::iterator iter_transition_vector;
    std::map< std::string, std::vector< std::vector< ExtTransition > > >::iterator iter_process_transition_map;
    std::map< std::string, std::map< std::string, std::vector< std::vector< ExtTransition > > > >::iterator iter_transition_map;
    std::vector< ExtTransition >::iterator iter_ext_transition_vector;

    std::string m_line;
    std::ostream *m_output;
    int m_indent;
    bool in_process, process_empty;

    void read( std::string path, std::vector< std::string > &definitions ) {
        std::unordered_map< std::string, dve::preprocessor::Definition > defs;
        try {
            for ( std::string & d : definitions ) {
                 dve::preprocessor::Definition def( d );
                 defs[ def.var ] = def;
            }
        }
        catch ( std::string error ) {
            std::cerr << error << std::endl;
            throw;
        }

        std::ifstream file;
        file.open( path.c_str() );
        dve::IOStream stream( file );
        dve::Lexer< dve::IOStream > lexer( stream );
        dve::Parser::Context ctx( lexer, path );
        try {
            ast = new parse::System( ctx );
            dve::preprocessor::System preproc( defs );
            preproc.process( *ast );
            ast->fold();
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
        std::ostream &out = *m_output;
        for ( int i = 0; i < m_indent; ++i )
            out << "    ";
        out << m_line << std::endl;
        m_line = "";
    }

    void line( std::string l = "" ) {
        append( l );
        outline();
    }

    void declare( std::vector< parse::Declaration > & decls,
                  std::vector < parse::ChannelDeclaration > & chandecls );
    void initVars( std::vector< parse::Declaration > & decls, std::string process );
    void start_process();
    void ensure_process();
    void end_process( std::string name );

    DveCompiler()
        : current_label(1), m_indent( 0 ),
          in_process( false ), process_empty( true )
    {}

    void insertTransition( parse::Process & proc, parse::Transition & trans );
    
    std::map< std::string, std::vector< parse::Transition * > >::iterator getTransitionVector( parse::Process &proc, parse::SyncExpr & chan );

    bool chanIsBuffered( parse::Process & proc, parse::SyncExpr & chan );

    void analyse_transition( parse::Process & p, parse::Transition * t,
                             std::vector< std::vector< ExtTransition > > & ext_transition_vector );
    void analyse();

    void write_C( parse::Expression & expr, std::ostream & ostr, std::string state_name, std::string context, std::string immcontext = "" );

    void write_C( parse::LValue & expr, std::ostream & ostr, std::string state_name, std::string context, std::string immcontext = "" );
    void write_C( parse::RValue & expr, std::ostream & ostr, std::string state_name, std::string context, std::string immcontext = "" );

    bool m_if_disjoint;
    bool m_if_empty;

    void boolexp_begin( bool disjoint ) {
        m_if_empty = true;
        m_if_disjoint = disjoint;
        append( " ( " );
    }

    void if_begin( bool disjoint ) {
        append( "if" );
        boolexp_begin( disjoint );
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

    void boolexp_end() {
        if_end();
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
              ( array ? ("[" + brick::string::fmt( array ) + "]") : "" ) +
              ";" );
    }

    std::string relate( std::string left, std::string op, std::string right ) {
        return left + " " + op + " " + right;
    }

    std::string process_state( parse::Process &p, std::string state ) {
        return state + "._control." + getProcName( p );
    }

    std::string channel_items( std::string p, parse::SyncExpr & chan, std::string state ) {
        return channel_items( getProcess( p ), chan, state );
    }

    std::string channel_items( parse::Process & p, parse::SyncExpr & chan, std::string state ) {
        parse::Identifier chanProc = getChannelProc( p, chan );
        if ( !chanProc.valid() )
            return state + "." + chan.chan.name() + ".number_of_items";
        else
            return state + "." + getProcName( chanProc.name() ) + "." + chan.chan.name() + ".number_of_items";
    }

    std::string channel_item_at( std::string p, parse::SyncExpr & chan, std::string pos, int x, std::string state ) {
        return channel_item_at( getProcess( p ), chan, pos, x, state );
    }

    std::string channel_item_at( parse::Process & p, parse::SyncExpr & chan, std::string pos, int x, std::string state ) {
        parse::Identifier chanProc = getChannelProc( p, chan );
        if ( !chanProc.valid() )
            return state + "." + chan.chan.name() + ".content[" + pos + "].x" + brick::string::fmt( x );
        else
            return state + "." + getProcName( chanProc.name() ) + "." + chan.chan.name() + ".content[" + pos + "].x" + brick::string::fmt( x );
    }

    parse::ChannelDeclaration & getChannel( std::string proc, parse::SyncExpr & chan ) {
        return getChannel( getProcess( proc ), chan );
    }

    parse::ChannelDeclaration & getChannel( parse::Process & proc, parse::SyncExpr & chan ) {
        if ( chan.proc.valid() ) {
            for ( parse::ChannelDeclaration &c : getProcess( chan.proc.ident.name() ).body.chandecls ) {
                if ( c.name == chan.chan.name() )
                    return c;
            }
        }
        else {
            for ( parse::ChannelDeclaration &c : proc.body.chandecls ) {
                if ( c.name == chan.chan.name() )
                    return c;
            }
            for ( parse::ChannelDeclaration &c : ast->chandecls ) {
                if ( c.name == chan.chan.name() )
                    return c;
            }
        }
        std::cerr << "ERROR: Couldn't find channel " << chan.chan.name() << std::endl;
        throw;
    }

    parse::Identifier getChannelProc( std::string proc, parse::SyncExpr & chan ) {
        return getChannelProc( getProcess( proc ), chan );
    }

    parse::Identifier getChannelProc( parse::Process & proc, parse::SyncExpr & chan ) {
        if ( chan.proc.valid() ) {
            for ( parse::ChannelDeclaration &c : getProcess( chan.proc.ident.name() ).body.chandecls ) {
                if ( c.name == chan.chan.name() )
                    return chan.proc.ident;
            }
        }
        else {
            for ( parse::ChannelDeclaration &c : proc.body.chandecls ) {
                if ( c.name == chan.chan.name() )
                    return proc.name;
            }
            for ( parse::ChannelDeclaration &c : ast->chandecls ) {
                if ( c.name == chan.chan.name() )
                    return parse::Identifier();
            }
        }
        std::cerr << "ERROR: Couldn't find channel " << chan.chan.name() << std::endl;
        throw;
    }

    std::string getVariable( std::string name, std::string context, std::string state ) {
        parse::Process &p = getProcess( context );
        for ( parse::Declaration &decl : p.body.decls ) {
            if ( decl.name == name )
                return ( decl.is_const ? "" : state + "." ) + getProcName( context ) + "." + name;
        }
        for ( parse::Declaration &decl : ast->decls ) {
            if ( decl.name == name )
                return ( decl.is_const ? "" : state + "." ) + name;
        }
        std::cerr << "ERROR: Couldn't find variable " << name << std::endl;
        throw;
    }

    int channel_capacity( std::string proc, parse::SyncExpr & chan ) {
        return channel_capacity( getProcess( proc ), chan );
    }

    int channel_capacity( parse::Process & proc, parse::SyncExpr & chan ) {
        return getChannel( proc, chan ).size;
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
        append( std::string( "l" ) + brick::string::fmt( current_label ) + ": " );
        current_label ++;
    }

    void block_begin() { line( "{" ); indent(); }
    void block_end() { deindent(); line( "}" ); }

    std::string in_state( parse::Process process, int state, std::string from_state ) {
        return "(" + process_state( process, from_state ) + " == " + brick::string::fmt( state ) + ")";
    }

    void setOutput( std::ostream &o ) {
        m_output = &o;
    }

    bool isAccepting( parse::Process &p, int stateID ) {
        for ( parse::Identifier &accept : p.body.accepts ) {
            if ( accept.name() == p.body.states[ stateID ].name() )
                return true;
        }
        return false;
    }

    bool isCommited( parse::Process &p, int stateID ) {
        for ( parse::Identifier &commit : p.body.commits ) {
            if ( commit.name() == p.body.states[ stateID ].name() )
                return true;
        }
        return false;
    }

    bool isCommited( parse::Process &p, std::string state ) {
        for ( parse::Identifier &commit : p.body.commits ) {
            if ( commit.name() == state )
                return true;
        }
        return false;
    }

    int getStateId( parse::Process &p, std::string state ) {
        int i = 0;
        for ( auto s : p.body.states ) {
            if ( s.name() == state )
                return i;
            ++ i;
        }
        return -1;
    }

    std::string getProcName( parse::Process &p ) {
        return getProcName( p.name.name() );
    }

    std::string getProcName( std::string proc ) {
        std::string retval = proc;
        std::transform( retval.begin(), retval.end(), retval.begin(),
                        []( int c ){
                            if ( ( c == '[' )
                                || ( c == ']' )
                                || ( c == '(' )
                                || ( c == ')' )
                                || ( c == ',' ) )
                                return int('_') ;
                            return c;
                        } );
        return retval;
    }

    parse::Process & getProcess( std::string proc ) {
        for ( parse::Process &p : ast->processes )
            if ( p.name.name() == proc )
                return p;
        for ( parse::Process &p : ast->properties )
            if ( p.name.name() == proc )
                return p;
        std::cerr << "ERROR: Couldn't find process " << proc << std::endl;
        throw;
    }

    parse::Property & getProperty( int i ) {
        if ( have_property ) {
            if ( i == 0 )
                return getProcess( ast->property.name() );
            return ast->properties[ i - 1 ];
        }
        return ast->properties[ i ];
    }

    int propCount() {
        return have_property ? ast->properties.size() + 1 : ast->properties.size();
    }

    void yield_state();
    void new_output_state();

    void gen_constants();
    void gen_successors( int property );
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
