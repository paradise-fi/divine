// -*- C++ -*- (c) 2011 Petr Rockai
#include <wibble/parse.h>
#include <divine/dve/lex.h>

#include <cassert>
#include <cstdlib> // atoi
#include <cstring> // atoi

#ifndef DIVINE_DVE_PARSE_H
#define DIVINE_DVE_PARSE_H

namespace divine {
namespace dve {

/*
 * Avoid dependent scope in the parsers, which would make them quite *
 * unreadable. XXX use a virtualised StreamBase or such instead...
 */

struct IOStream {
    std::istream &i;

    std::string remove() {
        char block[1024];
        i.read( block, 1023 );
        block[i.gcount()] = 0;
        return block;
    }

    bool eof() {
        return i.eof();
    }

    IOStream( std::istream &i ) : i( i ) {}
};

typedef wibble::Parser< Token, Lexer< IOStream > > Parser;

namespace parse {

struct Constant : Parser {
    Token token;
    int value;

    void sign() {
        eat( Token::Minus );
        value = -1;
    }

    Constant( Context &c ) : Parser( c ) {
        value = 1;
        maybe( &Constant::sign );
        token = eat( Token::Constant );

        if ( token.data == "true" )
            value = 1;
        else if ( token.data == "false" )
            value = 0;
        else
            value = value * atoi( token.data.c_str() );
    }
    Constant() {}
};

struct Identifier : Parser {
    Token token;
    std::string name() const { return token.data; }
    Identifier( Context &c ) : Parser( c ) {
        token = eat( Token::Identifier );
    }
    Identifier() {}
};

struct Expression;

struct RValue : Parser {
    Identifier ident;
    Constant value;
    Expression *idx;

    void subscript();
    void reference() {
        ident = Identifier( context() );
        maybe( &RValue::subscript );
    }

    void constant() {
        value = Constant( context() );
    }

    RValue( Context &c ) : Parser( c ), idx( 0 ) {
        either( &RValue::reference, &RValue::constant );
    }

    RValue() : idx( 0 ) {}
};

struct Expression : Parser {
    Token op;
    Expression *lhs, *rhs;
    RValue *rval;

    void parens() {
        eat( Token::ParenOpen );
    }

    // TODO: Unary operators: minus and tilde (bitwise negation)
    void negation() {
        op = eat( Token::Bool_Not );
        Expression _lhs( context() );
        lhs = new Expression( _lhs );
    }

    void rvalue() {
        RValue _rval( context() );
        rval = new RValue( _rval );
    }

    // TODO: Use the precedence climbing algorithm here, as it is likely
    // substantially more efficient
    Expression( Context &c, int prec = Token::precedences ) : Parser( c )
    {
        lhs = rhs = 0;
        rval = 0;

        if ( prec ) { // XXX: associativity
            Expression _lhs( context(), prec - 1 );
            lhs = new Expression( _lhs );
            op = eat();
            if ( op.precedence( prec ) ) {
                Expression _rhs( context(), prec );
                rhs = new Expression( _rhs );
            } else
                context().rewind( 1 );
        } else {
            if ( next( Token::ParenOpen ) ) {
                Expression _lhs( context() );
                eat( Token::ParenClose );
                lhs = new Expression( _lhs );
            } else {
                either( &Expression::negation,
                        &Expression::rvalue );
            }
        }

        if ( !lhs && !rval )
            fail( "expression" );

        /* simplify expressions */
        if ( lhs && !rhs && !op.precedence( prec ) ) {
            Expression ex = *lhs;
            delete lhs;
            *this = ex;
        }
    }

    Expression() : lhs( 0 ), rhs( 0 ), rval( 0 ) {}
};

inline void RValue::subscript() {
    eat( Token::IndexOpen );
    idx = new Expression( context() );
    eat( Token::IndexClose );
}

struct LValue : Parser {
    Identifier ident;
    Expression idx;

    void subscript() {
        eat( Token::IndexOpen );
        idx = Expression( context() );
        eat( Token::IndexClose );
    }

    LValue( Context &c ) : Parser( c ) {
        ident = Identifier( c );
        maybe( &LValue::subscript );
    }
    LValue() {}
};

struct Assignment : Parser {
    LValue lhs;
    Expression rhs;
    Assignment( Context &c ) : Parser( c ) {
        lhs = LValue( c );
        eat( Token::Assignment );
        rhs = Expression( c );
    }
};

struct SyncExpr : Parser {
    bool write;
    Identifier chan;
    Expression val;

    void value() {
        val = Expression( context() );
    }

    SyncExpr( Context &c ) : Parser( c ) {
        chan = Identifier( c );
        if ( next( Token::SyncWrite ) )
            write = true;
        else if ( next( Token::SyncRead ) )
            write = false;
        else
            fail( "sync read/write mark: ! or ?" );

        maybe( &SyncExpr::value );
    }

    SyncExpr() {}
};

/*
 * Never use this parser directly, only as an AST node. See Declarations.
 */
struct Declaration : Parser {
    bool is_const, is_chan;
    int width; // bytes
    bool is_array;
    int size;
    std::vector< Expression > initial;
    std::string name;

    void subscript() {
        eat( Token::IndexOpen );
        Token t = eat( Token::Constant );
        eat( Token::IndexClose );
        size = atoi( t.data.c_str() );
        is_array = true;
    }

    void initialiser() {
        Token t = eat( Token::Assignment );

        if ( is_array )
            list< Expression >( std::back_inserter( initial ),
                                Token::BlockOpen, Token::Comma, Token::BlockClose );
        else
            initial.push_back( Expression ( context() ) );
    }

    Declaration( Context &c ) : Parser( c )
    {
        Token t = eat( Token::Identifier );
        name = t.data;
        is_array = false;
        maybe( &Declaration::subscript );
        maybe( &Declaration::initialiser );
    }
};

/*
 * This parses a compact (comma-separated) declaration with a common type but
 * multiple identifiers. Always use this production, and never declaration
 * above. On the other hand, always use Declaration in the AST. See also
 * "declarations".
 */
struct Declarations : Parser {

    std::vector< Declaration > decls;

    Declarations( Context &c ) : Parser( c ) {
        bool is_const = false;
        bool is_chan = false;
        int width = 1; // whatever.

        if ( next( Token::Const ) )
            is_const = true;

        if ( next( Token::Int ) )
            width = 2;
        else if ( next( Token::Byte ) )
            width = 1;
        else if ( next( Token::Channel ) ) {
            is_chan = true;
        } else
            fail( "type name (int, byte, channel)" );

        list< Declaration >( std::back_inserter( decls ), Token::Comma );
        for ( unsigned i = 0; i < decls.size(); ++i ) {
            decls[i].is_const = is_const;
            decls[i].is_chan = is_chan;
            decls[i].width = width;
        }
        semicolon();
    }
};

struct Transition : Parser {
    Identifier from, to;
    std::vector< Expression > guards;
    std::vector< Assignment > effects;
    SyncExpr syncexpr;

    void guard() {
        eat( Token::Guard );
        list< Expression >( std::back_inserter( guards ), Token::Comma );
        semicolon();
    }

    void effect() {
        eat( Token::Effect );
        list< Assignment >( std::back_inserter( effects ), Token::Comma );
        semicolon();
    }

    void sync() {
        eat( Token::Sync );
        syncexpr = SyncExpr( context() );
        semicolon();
    }

    Transition( Context &c ) : Parser( c ) {
        from = Identifier( c );
        eat( Token::Arrow );
        to = Identifier( c );

        eat( Token::BlockOpen );
        maybe( &Transition::guard );
        maybe( &Transition::sync );
        maybe( &Transition::effect );
        eat( Token::BlockClose );
    }
};

static inline void declarations( Parser &p, std::vector< Declaration > &decls )
{
    std::vector< Declarations > declss;
    p.many< Declarations >( std::back_inserter( declss ) );
    for ( unsigned i = 0; i < declss.size(); ++i )
        std::copy( declss[i].decls.begin(), declss[i].decls.end(),
                   std::back_inserter( decls ) );
}

struct Process : Parser {
    Identifier name;
    std::vector< Declaration > decls;
    std::vector< Identifier > states, accepts, inits;
    std::vector< Transition > trans;

    void accept() {
        eat( Token::Accept );
        list< Identifier >( std::back_inserter( accepts ), Token::Comma );
        semicolon();
    }

    Process( Context &c ) : Parser( c ) {
        eat( Token::Process );
        name = Identifier( c );
        eat( Token::BlockOpen );

        declarations( *this, decls );

        eat( Token::State );
        list< Identifier >( std::back_inserter( states ), Token::Comma );
        semicolon();

        eat( Token::Init );
        list< Identifier >( std::back_inserter( inits ), Token::Comma );
        semicolon();

        maybe( &Process::accept );

        eat( Token::Trans );
        list< Transition >( std::back_inserter( trans ), Token::Comma );
        semicolon();

        eat( Token::BlockClose );
    }
};

struct System : Parser {
    std::vector< Declaration > decls;
    std::vector< Process > processes;
    bool synchronous;

    void property() {
        eat( Token::Property );
        eat( Token::Identifier);
    }

    System( Context &c ) : Parser( c )
    {
        synchronous = false;
        declarations( *this, decls );
        many< Process >( std::back_inserter( processes ) );
        eat( Token::System );

        if ( next( Token::Async ) ); // nothing
        else if ( next( Token::Sync ) )
            synchronous = true;
        else
            fail( "sync or async" );

        maybe( &System::property );
        semicolon();
    }
};

}
}
}

#endif
