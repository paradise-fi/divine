// -*- C++ -*- (c) 2013 Jan Kriho

#ifndef DIVINE_DVE_PREPROCESSOR_H
#define DIVINE_DVE_PREPROCESSOR_H

#include <divine/dve/parse.h>
#include <string>
#include <wibble/string.h>
#include <memory>
#include <unordered_map>

#include <divine/utility/buchi.h>

namespace divine {
namespace dve {

namespace preprocessor {

struct Definition {
    std::string var, value;
    std::shared_ptr< dve::Lexer< dve::IOStream > > lexer;
    std::shared_ptr< dve::IOStream > stream;
    std::shared_ptr< std::stringstream > realstream;

    Definition( std::string def ) {
        size_t pos = def.find( "=" );
        if ( pos == std::string::npos ) {
            throw std::string( "Invalid definition: " + def );
        }
        var = def.substr( 0, pos );
        value = def.substr( pos + 1, std::string::npos );

        realstream.reset( new std::stringstream() );
        (*realstream) << value;
        stream.reset( new dve::IOStream( *realstream ) );
        lexer.reset( new dve::Lexer< dve::IOStream >( *stream ) );
    }

    Definition() {}
};

typedef std::unordered_map< std::string, Definition > Definitions;

struct Macros {
    std::vector< parse::Macro< parse::Expression > > exprs;

    parse::Macro< parse::Expression > & getExpr( std::string name ) {
        for ( parse::Macro< parse::Expression > &macro : exprs ) {
            if ( macro.name.name() == name )
                return macro;
        }
        throw;
    }
};

struct Initialiser : Parser {
    std::vector< parse::Expression > initial;

    void initList() {
        list< parse::Expression >( std::back_inserter( initial ),
                                   Token::BlockOpen, Token::Comma,
                                   Token::BlockClose );
    }

    Initialiser( Context &c ) : Parser( c ) {
        if ( maybe( &Initialiser::initList ) )
            return;
        initial.push_back( parse::Expression( context() ) );
    }
};

struct Expression {
    Definitions &defs;
    Macros &macros;
    parse::Expression &expr;

    Expression( Definitions &ds, Macros &ms, parse::Expression &e )
        : defs( ds ), macros( ms ), expr( e )
    {
        if ( expr.rval && expr.rval->valid() && expr.rval->mNode.valid() ) {
            parse::Macro< parse::Expression > &m = macros.getExpr( expr.rval->mNode.name.name() );
            expr.op = m.content.op;
            if ( m.content.lhs )
                expr.lhs.reset( new parse::Expression( *m.content.lhs, true) );
            if ( m.content.rhs )
                expr.rhs.reset( new parse::Expression( *m.content.rhs, true) );
            if ( m.content.rval )
                expr.rval.reset( new parse::RValue( *m.content.rval, true) );
        }
        if ( expr.lhs )
            Expression( defs, macros, *expr.lhs );
        if ( expr.rhs )
            Expression( defs, macros, *expr.rhs );
        if ( expr.rval && expr.rval->idx )
            Expression( defs, macros, *expr.rval->idx );
    }
};

struct Transition {
    Definitions &defs;
    Macros &macros;
    parse::Transition &trans;

    Transition( Definitions &ds, Macros &ms, parse::Transition &t )
        : defs( ds ), macros( ms ), trans( t )
    {
        for ( parse::Expression &e : trans.guards )
            Expression( defs, macros, e );
    }
};

struct Automaton {
    Definitions &defs;
    Macros &macros;
    parse::Automaton &proc;

    Automaton( Definitions &ds, Macros &ms, parse::Automaton &p )
        : defs( ds ), macros( ms ), proc( p )
    {
        for ( parse::Transition &t : proc.trans )
            Transition( defs, macros, t );
    }
};

struct System {
    Definitions &defs;
    Macros macros;
    typedef std::vector< std::pair< int, int > > BATrans;

    parse::Property LTL2Process( parse::LTL &prop ) {
        parse::Property propBA;

        std::string ltl = prop.property.data.substr( 1, prop.property.data.length() - 2 );

        Buchi ba;
        std::vector< Buchi::DNFClause > clauses;
        ba.build( ltl,
                  [&]( Buchi::DNFClause &cls )
                  { clauses.push_back( cls ); return clauses.size() - 1; } );

        propBA.name = prop.name;
        propBA.ctx = prop.ctx;
        for ( int i = 0; i < ba.size(); i++ ) {
            propBA.states.push_back( parse::Identifier( "q" + wibble::str::fmt( i ), prop.context() ) );

            if ( ba.isAccepting( i ) )
                propBA.accepts.push_back( parse::Identifier( "q" + wibble::str::fmt( i ), prop.context() ) );

            const BATrans &batrans = ba.transitions( i );
            for ( std::pair< int, int > bt : batrans ) {
                parse::Transition t;
                t.ctx = prop.ctx;
                t.proc = prop.name;
                t.from = parse::Identifier( "q" + wibble::str::fmt( i ), prop.context() );
                t.to = parse::Identifier( "q" + wibble::str::fmt( bt.first ), prop.context() );
                for ( std::pair< bool, std::string > &item : clauses[ bt.second ] ) {
                    t.guards.push_back(
                        parse::Expression(
                            ( item.first ? "" : "not ") + item.second + "()",
                            prop.context() ) );
                }
                propBA.trans.push_back( t );
            }
        }
        propBA.inits.push_back( parse::Identifier( "q" + wibble::str::fmt( ba.getInitial() ), prop.context() ) );

        return propBA;
    }

    void processInput( parse::Declaration & decl ) {
        if ( defs.count( decl.name ) ) {
            Initialiser init( decl.context().createChild( *defs[ decl.name ].lexer, defs[ decl.name ].var ) );
            decl.initial = init.initial;
            decl.is_const = true;
            decl.is_input = false;
        }
        else if ( decl.initial.size() ) {
            decl.is_const = true;
            decl.is_input = false;
        }
    }

    void process( parse::System & ast ) {
        macros.exprs = ast.exprs;
        for ( parse::Declaration & decl : ast.decls ) {
            if ( decl.is_input ) {
                processInput( decl );
            }
        }
        for ( parse::LTL & ltl : ast.ltlprops ) {
            ast.properties.push_back( LTL2Process( ltl ) );
        }
        for ( parse::Automaton & proc : ast.processes ) {
            Automaton( defs, macros, proc );
        }

        for ( parse::Automaton & proc : ast.properties  ) {
            Automaton( defs, macros, proc );
        }
    }

    System( Definitions &defs ) : defs( defs ) {}
};

}

}
}

#endif