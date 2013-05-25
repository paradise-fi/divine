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

typedef std::unordered_map< std::string, int > Substitutions;

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

parse::Identifier getProcRef( parse::MacroNode& mnode, Definitions& defs, Macros& ms, SymTab& symtab, const Substitutions &substs );

struct Expression {
    enum Mode { Normal, ProcRef };
    Definitions &defs;
    Macros &macros;
    parse::Expression &expr;

    template< typename T >
    Substitutions getSubstitutions( parse::MacroNode &mn, const parse::Macro< T > &ma, SymTab symtab, const Substitutions &substs ) {
        Substitutions newsubsts;
        if ( mn.params.size() != ma.params.size() )
            mn.fail( "Parameter count mismatch", Parser::FailType::Semantic );
        for ( int i = 0; i < mn.params.size(); i++ ) {
            Expression( defs, macros, *mn.params[ i ], symtab, substs );
            dve::Expression ex( symtab, *mn.params[ i ] );
            EvalContext ctx;
            int v = ex.evaluate( ctx );
            newsubsts[ ma.params[ i ].param.name() ] = v;
        }
        return newsubsts;
    }

    void rvalMacro( Mode mode, SymTab &symtab, const Substitutions &substs ) {
        if ( mode == Normal ) {
            parse::Macro< parse::Expression > &m = macros.getExpr( expr.rval->mNode.name.name() );
            Substitutions newsubsts = getSubstitutions( expr.rval->mNode, m, symtab, substs );
            expr.op = m.content.op;
            if ( m.content.lhs )
                expr.lhs.reset( new parse::Expression( *m.content.lhs, parse::ASTClone() ) );
            else
                expr.lhs.reset();

            if ( m.content.rhs )
                expr.rhs.reset( new parse::Expression( *m.content.rhs, parse::ASTClone() ) );
            else
                expr.rhs.reset();

            if ( m.content.rval )
                expr.rval.reset( new parse::RValue( *m.content.rval, parse::ASTClone() ) );
            else
                expr.rval.reset();

            Expression( defs, macros, expr, symtab, newsubsts );
        }
        else if ( mode == ProcRef ) {
            expr.rval->ident = getProcRef( expr.rval->mNode, defs, macros, symtab, substs );
        }
    }

    Expression( Definitions &ds, Macros &ms, parse::Expression &e, SymTab &symtab, const Substitutions &substs )
        : Expression( ds, ms, e, Mode::Normal, symtab, substs ) {}

    Expression( Definitions &ds, Macros &ms, parse::Expression &e, Mode mode, SymTab &symtab, const Substitutions &substs )
        : defs( ds ), macros( ms ), expr( e )
    {
        if ( expr.rval && expr.rval->valid() && expr.rval->mNode.valid() ) {
            rvalMacro( mode, symtab, substs );
        }
        if ( expr.lhs ) {
            if ( expr.op.id == dve::TI::Period || expr.op.id == dve::TI::Arrow )
                Expression( defs, macros, *expr.lhs, Mode::ProcRef, symtab, substs );
            else
                Expression( defs, macros, *expr.lhs, symtab, substs );
        }
        if ( expr.rhs )
            Expression( defs, macros, *expr.rhs, symtab, substs );

        if ( expr.rval && expr.rval->idx )
            Expression( defs, macros, *expr.rval->idx, symtab, substs );
        else if ( expr.rval && expr.rval->ident.valid() ) {
            if ( substs.count( expr.rval->ident.name() ) ) {
                expr.rval->value = parse::Constant( substs.find( expr.rval->ident.name() )->second, expr.context() );
                expr.rval->ident.ctx = 0;
            }
        }
    }
};

struct SyncExpr {
    Definitions &defs;
    Macros &macros;
    parse::SyncExpr &syncexpr;

    SyncExpr( Definitions &ds, Macros &ms, parse::SyncExpr &sy, SymTab &symtab, const Substitutions &substs )
        : defs( ds ), macros( ms ), syncexpr( sy )
    {
        if ( syncexpr.proc.valid() && syncexpr.proc.mNode.valid() ) {
            syncexpr.proc.ident = getProcRef( syncexpr.proc.mNode, defs, macros, symtab, substs );
        }
    }
};

struct Transition {
    Definitions &defs;
    Macros &macros;
    parse::Transition &trans;

    Transition( Definitions &ds, Macros &ms, parse::Transition &t, SymTab &symtab, const Substitutions &substs )
        : defs( ds ), macros( ms ), trans( t )
    {
        if ( trans.syncexpr.valid() )
            SyncExpr( defs, macros, trans.syncexpr, symtab, substs );
        for ( parse::Expression &e : trans.guards )
            Expression( defs, macros, e, symtab, substs );
        for ( parse::Assignment &a : trans.effects ) {
            Expression( defs, macros, a.rhs, symtab, substs );
        }
    }
};


struct Declaration {
    Definitions &defs;
    Macros &macros;
    parse::Declaration &decl;

    Declaration( Definitions &ds, Macros &ms, parse::Declaration &d, SymTab &st, const Substitutions &substs )
        : defs( ds ), macros( ms ), decl( d )
    {
        if ( decl.sizeExpr.valid() )
            Expression( defs, macros, decl.sizeExpr, st, substs );

        if ( decl.is_input ) {
            if ( defs.count( decl.name ) ) {
                Initialiser init( decl.context().createChild( *defs[ decl.name ].lexer, defs[ decl.name ].var ) );
                decl.initialExpr = init.initial;
            }
            decl.is_const = true;
            decl.is_input = false;
        }

        for ( parse::Expression &expr : decl.initialExpr )
            Expression( defs, macros, expr, st, substs );

        if ( decl.is_const )
            decl.fold( &st );
    }
};

struct ChannelDeclaration {
    Definitions &defs;
    Macros &macros;
    parse::ChannelDeclaration &decl;

    ChannelDeclaration( Definitions &ds, Macros &ms, parse::ChannelDeclaration &d, SymTab &st, const Substitutions &substs )
        : defs( ds ), macros( ms ), decl( d )
    {
        if ( decl.sizeExpr.valid() )
            Expression( defs, macros, decl.sizeExpr, st, substs );
    }
};


struct Automaton {
    Definitions &defs;
    Macros &macros;
    parse::Automaton &proc;
    SymTab symtab;

    Automaton( Definitions &ds, Macros &ms, parse::Automaton &p, SymTab &st, const Substitutions &substs )
        : defs( ds ), macros( ms ), proc( p ), symtab( &st )
    {
        for ( parse::Declaration & decl : proc.decls ) {
                Declaration( defs, macros, decl, symtab, substs );
        }
        for ( parse::ChannelDeclaration & decl : proc.chandecls ) {
                ChannelDeclaration( defs, macros, decl, symtab, substs );
        }
        for ( parse::Transition &t : proc.trans )
            Transition( defs, macros, t, symtab, substs );
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

    void makeProcess( parse::MacroNode &mn, parse::System & ast ) {
        std::vector< int > idents;
        Substitutions substs;
        parse::Macro< parse::Automaton > &ma = macros.getProcess( mn.name.name() );
        if ( mn.params.size() != ma.params.size() )
            mn.fail( "Parameter count mismatch", Parser::FailType::Semantic );
        for ( int i = 0; i < mn.params.size(); i++ ) {
            Expression( defs, macros, *mn.params[ i ], symtab, Substitutions() );
            dve::Expression ex( symtab, *mn.params[ i ] );
            EvalContext ctx;
            int v = ex.evaluate( ctx );
            if ( ma.params[ i ].key )
                idents.push_back( v );
            substs[ ma.params[ i ].param.name() ] = v;
        }
        std::stringstream str;
        str << mn.name.name() << "(";
        bool tail = false;
        for ( int &v : idents ) {
            if ( tail )
                str << ",";
            str << v;
            tail = true;
        }
        str << ")";
        ast.processes.push_back( parse::Automaton( ma.content, parse::ASTClone() ) );
        ast.processes.back().setName( parse::Identifier( str.str(), ast.context() ) );
        Automaton( defs, macros, ast.processes.back(), symtab, substs );
    }

    void process( parse::System & ast ) {
        macros.exprs = ast.exprs;
        macros.processes = ast.templates;

        for ( parse::Declaration & decl : ast.decls ) {
                Declaration( defs, macros, decl, symtab, Substitutions() );
        }
        for ( parse::ChannelDeclaration & decl : ast.chandecls ) {
                ChannelDeclaration( defs, macros, decl, symtab, Substitutions() );
        }

        for ( parse::LTL & ltl : ast.ltlprops ) {
            ast.properties.push_back( LTL2Process( ltl ) );
        }
        for ( parse::Automaton & proc : ast.processes ) {
            Automaton( defs, macros, proc, symtab, Substitutions() );
        }

        for ( parse::Automaton & proc : ast.properties  ) {
            Automaton( defs, macros, proc, symtab, Substitutions() );
        }
        for ( parse::MacroNode & mn : ast.procInstances ) {
            makeProcess( mn, ast );
        }
    }

    System( Definitions &defs ) : defs( defs ) {}
};

}

}
}

#endif