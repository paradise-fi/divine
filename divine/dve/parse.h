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

    std::ostream& dump( std::ostream &o );

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
        Expression _lhs( context(), 0 );
        lhs = new Expression( _lhs );
    }

    void rvalue() {
        RValue _rval( context() );
        rval = new RValue( _rval );
    }

    std::ostream& dump( std::ostream &o ) {
        if ( lhs && rhs ) {
            o << "(";
            lhs->dump( o );
            o << ") " << op.data << " (";
            rhs->dump( o );
            o << ")";
        }
        else if ( lhs ) {
            o << op.data;
            lhs->dump( o );
        }
        else {
            rval->dump( o );
        }
        return o;
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
        if ( lhs && !rhs && !op.precedence( prec ) && !op.unary() ) {
            Expression ex = *lhs;
            delete lhs;
            *this = ex;
        }
    }

    Expression() : lhs( 0 ), rhs( 0 ), rval( 0 ) {}
};

struct ExpressionList : Parser {
    std::vector< Expression > explist;
    bool compound;
    
    std::ostream& dump( std::ostream &o ) {
        o << "{";
        for( auto it = explist.begin(); it != explist.end(); it++ ) {
            if ( it != explist.begin() )
                o << ",";
            it->dump( o );
        }
        o << "}";
        return o;
    }

    ExpressionList( Context &c ) : Parser( c )
    {
        if ( next( Token::BlockOpen ) ) {
                compound = true;
                list< Expression >( std::back_inserter( explist ), Token::Comma );
                eat( Token::BlockClose );
            }
            else
                explist.push_back( Expression( context() ) );
    }
    
    ExpressionList() {};
    
};

inline void RValue::subscript() {
    eat( Token::IndexOpen );
    idx = new Expression( context() );
    eat( Token::IndexClose );
}

inline std::ostream& RValue::dump( std::ostream &o ) {
    if ( ident.valid() ) {
        o << ident.name();
    }
    else {
        o << value.value;
    }
    return o;
}

struct LValue : Parser {
    Identifier ident;
    Expression idx;

    void subscript() {
        eat( Token::IndexOpen );
        idx = Expression( context() );
        eat( Token::IndexClose );
    }

    std::ostream& dump( std::ostream &o ) {
        o << ident.name();
        return o;
    }

    LValue( Context &c ) : Parser( c ) {
        ident = Identifier( c );
        maybe( &LValue::subscript );
    }
    LValue() {}
};

struct LValueList: Parser {
    std::vector< LValue > lvlist;
    bool compound;
    
    std::ostream& dump( std::ostream &o ) {
        o << "{";
        for( auto it = lvlist.begin(); it != lvlist.end(); it++ ) {
            if ( it != lvlist.begin() )
                o << ",";
            it->dump( o );
        }
        o << "}";
        return o;
    }

    LValueList( Context &c ) : Parser( c )
    {
        if ( next( Token::BlockOpen ) ) {
                compound = true;
                list< LValue >( std::back_inserter( lvlist ), Token::Comma );
                eat( Token::BlockClose );
            }
            else
                lvlist.push_back( LValue( context() ) );
    }
    
    LValueList() {}
};

struct Assignment : Parser {
    LValue lhs;
    Expression rhs;

    std::ostream& dump( std::ostream &o ) {
        lhs.dump( o );
        o << " = ";
        rhs.dump( o );
        return o;
    }

    Assignment( Context &c ) : Parser( c ) {
        lhs = LValue( c );
        eat( Token::Assignment );
        rhs = Expression( c );
    }
};

struct SyncExpr : Parser {
    bool write;
    bool compound;
    Identifier chan;
    ExpressionList exprlist;
    LValueList lvallist;

    void lvalues() {
        lvallist = LValueList( context() );
    }

    void expressions() {
        exprlist = ExpressionList( context() );
    }

    std::ostream& dump( std::ostream &o ) {
        o << chan.name();
        if ( write ) {
            o << "!";
            if ( exprlist.valid() )
                exprlist.dump( o );
        }
        else {
            o << "?";
            if ( lvallist.valid() )
                lvallist.dump( o );
        }
        return o;
    }

    SyncExpr( Context &c ) : Parser( c ) {
        chan = Identifier( c );
        if ( next( Token::SyncWrite ) )
            write = true;
        else if ( next( Token::SyncRead ) )
            write = false;
        else
            fail( "sync read/write mark: ! or ?" );

        if ( write )
            maybe( &SyncExpr::expressions );
        else
            maybe( &SyncExpr::lvalues );
    }

    SyncExpr() {}
};


/*
 * Never use this parser directly, only as an AST node. See Declarations.
 */
struct Declaration : Parser {
    bool is_const, is_compound, is_buffered;
    int width; // bytes
    bool is_array;
    int size;
    std::vector< int > components;
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
            initial.push_back( Expression( context() ) );
    }

  Declaration( Context &c ) : Parser( c ), is_buffered( 0 ), size( 1 )
    {
        Token t = eat( Token::Identifier );
        name = t.data;
        is_array = false;
        maybe( &Declaration::subscript );
        maybe( &Declaration::initialiser );
    }
};

struct Type : Parser {
    enum TypeSize { Byte = 1, Int = 2 };
    TypeSize size;
    
    Type( Context &c ) : Parser( c ) {
        if ( next( Token::Byte ) ) {
            size = TypeSize::Byte;
        }
        else if ( next ( Token::Int ) ) {
            size = TypeSize::Int;
        }
        else
            fail( "type name (int, byte)" );
    }
};

struct ChannelDeclaration : Parser {
    bool is_buffered, is_const;
    int size, width;
    std::string name;
    bool is_compound;
    std::vector< int > components;

    void subscript() {
        eat( Token::IndexOpen );
        Token t = eat( Token::Constant );
        eat( Token::IndexClose );
        size = atoi( t.data.c_str() );
        is_buffered = (size > 0);
    }

    ChannelDeclaration( Context &c ) : Parser( c ), size( 1 )
    {
        Token t = eat( Token::Identifier );
        name = t.data;
        is_buffered = false;
        maybe( &ChannelDeclaration::subscript );
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
    std::vector< ChannelDeclaration > chandecls;
    std::vector< Type > types;

    bool is_const = false;
    bool is_chan = false;
    bool is_compound = false;
    int width = 2; // default for untyped channels

    template< typename T >
    void init( T &decllist )
    {
        for ( unsigned i = 0; i < decllist.size(); ++i ) {
            decllist[i].is_const = is_const;
            //decls[i].is_chan = is_chan;
            decllist[i].is_compound = is_compound;
            for( unsigned j = 0; j < types.size(); ++j ) {
                decllist[i].components.push_back( types[i].size );
                width += types[i].size;
            }
            decllist[i].width = width;
        }
    }

    Declarations( Context &c ) : Parser( c ) {
        if ( next( Token::Const ) )
            is_const = true;

        if ( next( Token::Int ) )
            width = 2;
        else if ( next( Token::Byte ) )
            width = 1;
        else if ( next( Token::Channel ) ) {
            is_chan = true;
            if ( next( Token::BlockOpen ) ) {
                width = 0;
                is_compound = true;
                list< Type >( std::back_inserter( types ), Token::Comma );
                eat( Token::BlockClose );
            }
        } else
            fail( "type name (int, byte, channel)" );
        if ( is_chan ) {
            list< ChannelDeclaration >( std::back_inserter( chandecls ), Token::Comma );
            init( chandecls );
        }
        else {
            list< Declaration >( std::back_inserter( decls ), Token::Comma );
            init( decls );
        }
        semicolon();
    }
};

struct Assertion : Parser {
    Identifier state;
    Expression expr;

    Assertion( Context &c ) : Parser( c ) {
        state = Identifier( c );
        colon();
        expr = Expression( c );
    }
};

struct Transition : Parser {
    Identifier from, to;
    Identifier proc;
    std::vector< Expression > guards;
    std::vector< Assignment > effects;
    SyncExpr syncexpr;
    Token end;

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

    std::ostream& dump( std::ostream &o ) {
        o << from.name() << " -> " << to.name() << " { ";
        o << "guard ";
        for ( auto grd = guards.begin(); grd != guards.end(); grd++ ) {
            grd->dump( o );
            o << ",";
        }
        o << "; sync ";
        if ( syncexpr.valid() )
            syncexpr.dump( o );
        o << "; effect ";
        for ( auto eff = effects.begin(); eff != effects.end(); eff++) {
            eff->dump( o );
            o << ",";
        }
        o << ";}";
        return o;
    }

    Transition( Context &c ) : Parser( c ) {
        from = Identifier( c );
        eat( Token::Arrow );
        to = Identifier( c );

        eat( Token::BlockOpen );
        maybe( &Transition::guard );
        maybe( &Transition::sync );
        maybe( &Transition::effect );

        end = eat( Token::BlockClose );
    }
};

static inline void declarations( Parser &p, std::vector< Declaration > &decls, std::vector< ChannelDeclaration > &chandecls )
{
    std::vector< Declarations > declss;
    p.many< Declarations >( std::back_inserter( declss ) );
    for ( unsigned i = 0; i < declss.size(); ++i ) {
        std::copy( declss[i].decls.begin(), declss[i].decls.end(),
                   std::back_inserter( decls ) );
        std::copy( declss[i].chandecls.begin(), declss[i].chandecls.end(),
                   std::back_inserter( chandecls ) );
    }
}

template< bool IsProperty >
struct Automaton : Parser {
    Identifier name;
    std::vector< Declaration > decls;
    std::vector< ChannelDeclaration > chandecls;
    std::vector< Identifier > states, accepts, commits, inits;
    std::vector< Assertion > asserts;
    std::vector< Transition > trans;

    void accept() {
        eat( Token::Accept );
        list< Identifier >( std::back_inserter( accepts ), Token::Comma );
        semicolon();
    }

    void commit() {
        eat( Token::Commit );
        list< Identifier >( std::back_inserter( commits ), Token::Comma );
        semicolon();
    }
    
    void optionalComma() {
        maybe( Token::Comma );
    }

    Automaton( Context &c ) : Parser( c ) {
        eat( IsProperty ? Token::Property : Token::Process );
        name = Identifier( c );
        eat( Token::BlockOpen );

        declarations( *this, decls, chandecls );

        eat( Token::State );
        list< Identifier >( std::back_inserter( states ), Token::Comma );
        semicolon();

        maybe( &Automaton::commit );
        maybe( &Automaton::accept );
        maybe( &Automaton::commit );

        eat( Token::Init );
        list< Identifier >( std::back_inserter( inits ), Token::Comma );
        semicolon();

        if ( next( Token::Assert ) ) {
            list< Assertion >( std::back_inserter( asserts ), Token::Comma );
            semicolon();
        }

        maybe( &Automaton::commit );
        maybe( &Automaton::accept );
        maybe( &Automaton::commit );

        if ( next( Token::Trans ) ) {
            list< Transition >( std::back_inserter( trans ), &Automaton::optionalComma );
            maybe( &Automaton::semicolon );
        }

        eat( Token::BlockClose );

        for( Transition &t : trans ) {
            t.proc = name;
        }
    }
};

typedef Automaton< false > Process;
typedef Automaton< true > Property;

struct System : Parser {
    std::vector< Declaration > decls;
    std::vector< ChannelDeclaration > chandecls;
    std::vector< Process > processes;
    std::vector< Property > properties;
    Identifier property;
    bool synchronous;

    void _property() {
        eat( Token::Property );
        property = Identifier( context() );
    }

    System( Context &c ) : Parser( c )
    {
        synchronous = false;
        declarations( *this, decls, chandecls );

        many< Process >( std::back_inserter( processes ) );
        many< Property >( std::back_inserter( properties ) );

        eat( Token::System );

        if ( next( Token::Async ) ); // nothing
        else if ( next( Token::Sync ) )
            synchronous = true;
        else
            fail( "sync or async" );

        maybe( &System::_property );
        semicolon();
    }
};

}
}
}

#endif
