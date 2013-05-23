// -*- C++ -*- (c) 2013 Jan Kriho

#ifndef DIVINE_DVE_PREPROCESSOR_H
#define DIVINE_DVE_PREPROCESSOR_H

#include <divine/dve/parse.h>
#include <divine/dve/interpreter.h>
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
    std::vector< parse::Macro< parse::Automaton > > processes;

    parse::Macro< parse::Expression > & getExpr( std::string name ) {
        for ( parse::Macro< parse::Expression > &macro : exprs ) {
            if ( macro.name.name() == name )
                return macro;
        }
        throw;
    }

    parse::Macro< parse::Automaton > & getProcess( std::string name ) {
        for ( parse::Macro< parse::Automaton > &macro : processes ) {
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

    Expression( Definitions &ds, Macros &ms, parse::Expression &e, SymTab &symtab )
        : defs( ds ), macros( ms ), expr( e )
    {
        if ( expr.rval && expr.rval->valid() && expr.rval->mNode.valid() ) {
            parse::Macro< parse::Expression > &m = macros.getExpr( expr.rval->mNode.name.name() );
            expr.op = m.content.op;
            if ( m.content.lhs )
                expr.lhs.reset( new parse::Expression( *m.content.lhs, parse::ASTClone() ) );
            if ( m.content.rhs )
                expr.rhs.reset( new parse::Expression( *m.content.rhs, parse::ASTClone() ) );
            if ( m.content.rval )
                expr.rval.reset( new parse::RValue( *m.content.rval, parse::ASTClone() ) );
        }
        if ( expr.lhs )
            Expression( defs, macros, *expr.lhs, symtab );
        if ( expr.rhs )
            Expression( defs, macros, *expr.rhs, symtab );
        if ( expr.rval && expr.rval->idx )
            Expression( defs, macros, *expr.rval->idx, symtab );

        // Transform instantiated processes names
        if ( ( expr.op.id == dve::TI::Period )
             || ( expr.op.id == dve::TI::Arrow ) )
        {
            if ( expr.lhs->rval->idx ) {
                EvalContext ctx;
                dve::Expression ex( symtab, *expr.lhs->rval->idx );
                int n = ex.evaluate( ctx );
                expr.lhs->rval->ident = parse::Identifier(
                    expr.lhs->rval->ident.name() + "[" + wibble::str::fmt( n ) + "]",
                    expr.context()
                );
            }
        }
    }
};

struct SyncExpr {
    Definitions &defs;
    Macros &macros;
    parse::SyncExpr &syncexpr;

    SyncExpr( Definitions &ds, Macros &ms, parse::SyncExpr &sy, SymTab &symtab )
        : defs( ds ), macros( ms ), syncexpr( sy )
    {
        if ( syncexpr.proc.valid() && syncexpr.proc.idx ) {
            Expression( defs, macros, *syncexpr.proc.idx, symtab );
            EvalContext ctx;
            dve::Expression e( symtab, *syncexpr.proc.idx );
            int n = e.evaluate( ctx );
            syncexpr.proc.ident = parse::Identifier(
                syncexpr.proc.ident.name() + "[" + wibble::str::fmt( n ) + "]",
                    syncexpr.context()
            );
        }
    }
};

struct Transition {
    Definitions &defs;
    Macros &macros;
    parse::Transition &trans;

    Transition( Definitions &ds, Macros &ms, parse::Transition &t, SymTab &symtab )
        : defs( ds ), macros( ms ), trans( t )
    {
        if ( trans.syncexpr.valid() )
            SyncExpr( defs, macros, trans.syncexpr, symtab );
        for ( parse::Expression &e : trans.guards )
            Expression( defs, macros, e, symtab );
        for ( parse::Assignment &a : trans.effects ) {
            Expression( defs, macros, a.rhs, symtab );
        }
    }
};


struct Declaration {
    Definitions &defs;
    Macros &macros;
    parse::Declaration &decl;

    Declaration( Definitions &ds, Macros &ms, parse::Declaration &d, SymTab &st )
        : defs( ds ), macros( ms ), decl( d )
    {
        if ( decl.sizeExpr.valid() )
            Expression( defs, macros, decl.sizeExpr, st );

        if ( decl.is_input ) {
            if ( defs.count( decl.name ) ) {
                Initialiser init( decl.context().createChild( *defs[ decl.name ].lexer, defs[ decl.name ].var ) );
                decl.initialExpr = init.initial;
            }
            decl.is_const = true;
            decl.is_input = false;
        }

        for ( parse::Expression &expr : decl.initialExpr )
            Expression( defs, macros, expr, st );
    }
};

struct ChannelDeclaration {
    Definitions &defs;
    Macros &macros;
    parse::ChannelDeclaration &decl;

    ChannelDeclaration( Definitions &ds, Macros &ms, parse::ChannelDeclaration &d, SymTab &st )
        : defs( ds ), macros( ms ), decl( d )
    {
        if ( decl.sizeExpr.valid() )
            Expression( defs, macros, decl.sizeExpr, st );
    }
};


struct Automaton {
    Definitions &defs;
    Macros &macros;
    parse::Automaton &proc;
    SymTab symtab;

    Automaton( Definitions &ds, Macros &ms, parse::Automaton &p, SymTab &st )
        : defs( ds ), macros( ms ), proc( p ), symtab( &st )
    {
        for ( parse::Declaration & decl : proc.decls ) {
                Declaration( defs, macros, decl, symtab );
        }
        for ( parse::ChannelDeclaration & decl : proc.chandecls ) {
                ChannelDeclaration( defs, macros, decl, symtab );
        }
        for ( parse::Transition &t : proc.trans )
            Transition( defs, macros, t, symtab );
    }
};

struct System {
    Definitions &defs;
    Macros macros;
    typedef std::vector< std::pair< int, int > > BATrans;
    SymTab symtab;

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

    void process( parse::System & ast ) {
        macros.exprs = ast.exprs;
        macros.processes = ast.templates;

        for ( parse::Declaration & decl : ast.decls ) {
                Declaration( defs, macros, decl, symtab );
        }
        for ( parse::ChannelDeclaration & decl : ast.chandecls ) {
                ChannelDeclaration( defs, macros, decl, symtab );
        }
        for ( parse::LTL & ltl : ast.ltlprops ) {
            ast.properties.push_back( LTL2Process( ltl ) );
        }
        for ( parse::MacroNode & mn : ast.procInstances ) {
            parse::Macro< parse::Automaton > &ma = macros.getProcess( mn.name.name() );
            ast.processes.push_back( parse::Automaton( ma.content, parse::ASTClone() ) );
            ast.processes.back().setName( parse::Identifier(
                ma.name.name() + "[" + wibble::str::fmt( ma.used ++) + "]",
                ast.context()
            ) );
        }
        for ( parse::Automaton & proc : ast.processes ) {
            Automaton( defs, macros, proc, symtab );
        }

        for ( parse::Automaton & proc : ast.properties  ) {
            Automaton( defs, macros, proc, symtab );
        }
    }

    System( Definitions &defs ) : defs( defs ) {}
};

}

}
}

#endif