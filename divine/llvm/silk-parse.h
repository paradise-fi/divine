// -*- C++ -*- (c) 2014 Petr Rockai

#define NO_RTTI

#include <brick-parse.h>
#include <brick-types.h>

#ifndef DIVINE_LLVM_SILK_PARSE_H
#define DIVINE_LLVM_SILK_PARSE_H

namespace divine {

/*
 * A language of predicates over individual program states. Supports mapping
 * and folding over threads, stacks and frames.
 */

namespace silk {

namespace parse {

enum class TI {
    Invalid, Comment, Newline, Application, Identifier, Constant,
    Map, Fold, SubScope, LambdaParen, Bind,
    ParenOpen, ParenClose,
    If, Then, Else,
    Bool_Or, Bool_And, Bool_Not,
    LEq, Lt, GEq, Gt, Eq, NEq,
    Plus, Minus, Mult, Div, Mod, Or, And, Xor, LShift, RShift, Arrow,
    Semicolon, Colon, BlockDef, BlockEnd, BraceOpen, BraceClose
};

const int TI_End = int( TI::BraceClose ) + 1;

struct Token : brick::parse::Token< TI >
{
    static const TI Comment = TI::Comment;
    static const std::string tokenName[TI_End];

    static const std::string frag[6];

    static const int precedences = 5;

    bool leftassoc() const {
        return true;
    }

    bool typeOp() const { return id == TI::Arrow; }

    int precedence() const
    {
        if ( id == TI::And || id == TI::Or || id == TI::Xor ||
             id == TI::LShift || id == TI::RShift || id == TI::Arrow )
            return 5;

        if ( id == TI::Mult || id == TI::Div || id == TI::Mod )
            return 4;

        if ( id == TI::Plus || id == TI::Minus )
            return 3;

        if ( id == TI::Eq || id == TI::NEq || id == TI::LEq ||
             id == TI::Lt || id == TI::Gt || id == TI::GEq )
            return 2;

        if ( id == TI::Bool_And || id == TI::Bool_Or )
            return 1;

        return 0;
    }

    Token( TI id, std::string data ) : brick::parse::Token< TI >( id, data ) {}
    Token() = default;
};

template< typename Stream >
struct Lexer : brick::parse::Lexer< Token, Stream >
{
    static int space( int c ) { return isspace( c ) && c != '\n'; }
    static int firstident( int c ) { return isalpha( c ) || c == '_'; }
    static int restident( int c ) {
        return isprint( c ) && !isspace( c ) &&
            c != '|' && c != '#' && c != ':' && c != '(' && c != ')' &&
            c != '{' && c != '}' && c != '.';
    }

    static constexpr const std::string * const frag = Token::frag;
    enum Frag { HASH, NL, AND, OR, TRUE, FALSE };

    Token remove() {
        this->skipWhile( space );

        this->match( frag[ NL ], TI::Newline );
        this->match( frag[ HASH ], frag[ NL ], TI::Comment );

        this->match( frag[ TRUE ], TI::Constant );
        this->match( frag[ FALSE ], TI::Constant );

        for ( int i = static_cast< int >( TI::Map ); i < TI_End; ++i )
            this->match( Token::tokenName[i], static_cast< TI >( i ) );

        this->match( frag[ OR ], TI::Bool_Or );
        this->match( frag[ AND ], TI::Bool_And );

        this->match( firstident, restident, TI::Identifier );
        this->match( isdigit, isdigit, TI::Constant );

        return this->decide();
    }

    Lexer( Stream &s )
        : brick::parse::Lexer< Token, Stream >( s )
    {}
};

struct Buffer {
    std::string buf;

    std::string remove() {
        std::string ret;
        std::swap( ret, buf );
        return ret;
    }

    bool eof() { return buf.empty(); }
    Buffer( std::string s ) : buf( s ) {}
};

typedef brick::parse::Parser< Token, Lexer< Buffer > > Parser;

struct Expression;
using ExpressionPtr = std::shared_ptr< Expression >;

struct Scope;
using ScopePtr = std::shared_ptr< Scope >;

struct TypeExpression;
using TypeExpressionPtr = std::shared_ptr< TypeExpression >;

struct Identifier : Parser {
    std::string name;
    Identifier() = default;
    Identifier( Context &c ) : Parser( c ) {
        auto t = eat( TI::Identifier );
        name = t.data;
    }
    Identifier( std::string n ) : name( n ) {}
};

template< typename T >
struct _BinOp {
    using Ptr = std::shared_ptr< T >;
    Ptr lhs, rhs;
    TI op;
    _BinOp( Ptr lhs, Token op, Ptr rhs )
        : lhs( lhs ), rhs( rhs ), op( op.id )
    {}
};

template< typename T >
struct _Application {
    using Ptr = std::shared_ptr< T >;
    Ptr lhs, rhs;
    _Application( Ptr lhs, Ptr rhs )
        : lhs( lhs ), rhs( rhs )
    {}
};

using Application = _Application< Expression >;
using BinOp = _BinOp< Expression >;

using TypeApplication = _Application< TypeExpression >;
using TypeBinOp = _BinOp< TypeExpression >;

struct TypeExpression : Parser
{
    using E = brick::types::Union< TypeBinOp, Identifier, TypeApplication >;
    E e;

    struct Atom {};

    TypeExpression( E e ) : e( e ) {}

    TypeExpression( Context &c, Atom ) : Parser( c ) {
        if ( next( TI::ParenOpen ) ) {
            e = TypeExpression( c ).e;
            eat( TI::ParenClose );
        } else {
            e = Identifier( c );
        }
    }

    TypeExpression( Context &c, int min_prec = 0 )
        : Parser( c )
    {
        int next_min_prec;
        bool done = false;

        e = TypeExpression( c, Atom() ).e;

        do {
            Token op = eat( false );

            if ( !op.valid() || !op.typeOp() ) {
                c.rewind( 1 );
                done = true;
                if ( op.valid() && !op.precedence() && Token::precedences + 1 >= min_prec &&
                     maybe( [&]() {
                             auto rhs = std::make_shared< TypeExpression >( c, Token::precedences + 2 );
                             next_min_prec = Token::precedences + 2;
                             this->e = TypeApplication( std::make_shared< TypeExpression >( e ), rhs );
                         } ) )
                    done = false;
            } else if ( op.precedence() >= min_prec ) {
                next_min_prec = op.precedence() + op.leftassoc();
                auto rhs = std::make_shared< TypeExpression >( c, next_min_prec );
                e = TypeBinOp( std::make_shared< TypeExpression >( e ), op, rhs );
            }
        } while ( !done );
    }
};

struct Pattern : Parser {
    TypeExpressionPtr type;
    Identifier binding;
    Pattern( Context &c ) : Parser( c ) {
        binding = Identifier( c );
        if ( next( TI::Colon ) )
            type = std::make_shared< TypeExpression >( c );
    }
    Pattern( Identifier i ) : binding( i ) {}
    Pattern() {}
    std::string name() { return binding.name; }
};

struct Constant : Parser
{
    int value;
    ScopePtr scope;

    Constant() = default;
    Constant( Context &c ) : Parser( c ) {
        if ( next( TI::BraceOpen ) ) {
            scope = std::make_shared< Scope >( c );
            eat( TI::BraceClose );
        } else if ( next( TI::BlockDef ) ) {
            scope = std::make_shared< Scope >( c );
            eat( TI::BlockEnd );
        } else {
            auto t = eat( TI::Constant );
            value = atoi( t.data.c_str() );
        }
    }
};

struct Lambda : Parser {
    Pattern bind;
    ExpressionPtr body;

    Lambda() = default;
    Lambda( Context &c );
    Lambda( Identifier i, ExpressionPtr b )
        : bind( i ), body( b )
    {}
};

struct IfThenElse : Parser {
    ExpressionPtr cond, yes, no;

    IfThenElse() = default;
    IfThenElse( Context &c );
};

struct SubScope {
    ExpressionPtr lhs, rhs;
    SubScope( ExpressionPtr lhs, ExpressionPtr rhs )
        : lhs( lhs ), rhs( rhs )
    {}
};

struct Expression : Parser {

    using E = brick::types::Union< BinOp, SubScope, Lambda, Constant, Identifier, IfThenElse, Application >;
    E e;

    struct Atom {};

    void lambda() { e = Lambda( context() ); }
    void variable() { e = Identifier( context() ); }
    void constant() { e = Constant( context() ); }
    void ifthenelse() { e = IfThenElse( context() ); }

    Expression() = default;
    Expression( E e ) : e( e ) {}
    Expression( Context &c, Atom ) : Parser( c ) {
        if ( next( TI::ParenOpen ) ) {
            e = Expression( c ).e;
            eat( TI::ParenClose );
        } else
            either( &Expression::constant,
                    &Expression::variable );
    }

    Expression( Context &c, int min_prec = 0 )
        : Parser( c )
    {
        if ( !min_prec ) {
            if ( !maybe( &Expression::lambda,
                         &Expression::ifthenelse ) )
                e = Expression( c, 1 ).e;
            return;
        }

        int next_min_prec;
        bool done = false;

        e = Expression( c, Atom() ).e;

        do {
            Token op = eat( false );

            if ( op.valid() && op.precedence() >= min_prec ) {
                next_min_prec = op.precedence() + op.leftassoc();
                auto rhs = std::make_shared< Expression >( c, next_min_prec );
                e = BinOp( std::make_shared< Expression >( e ), op, rhs );
            } else if ( op.valid() && op.id == TI::SubScope && Token::precedences + 1 >= min_prec ) {
                auto rhs = std::make_shared< Expression >( c, Token::precedences + 2 );
                next_min_prec = Token::precedences + 2;
                e = SubScope( std::make_shared< Expression >( e ), rhs );
            } else {
                c.rewind( 1 ); // put back op
                done = true;
                if ( op.valid() && !op.precedence() && Token::precedences + 1 >= min_prec &&
                     maybe( [&]() {
                             auto rhs = std::make_shared< Expression >( c, Token::precedences + 2 );
                             next_min_prec = Token::precedences + 2;
                             this->e = Application( std::make_shared< Expression >( e ), rhs );
                         } ) )
                    done = false;
            }
        } while ( !done );
    }
};

struct PrintTo {
    std::ostream &o;
    template< typename T >
    auto operator()( T t ) -> decltype( o << t ) { return o << t; }
    PrintTo( std::ostream &o ) : o( o ) {}
};

inline std::ostream &operator<<( std::ostream &o, Expression e );

inline std::ostream &operator<<( std::ostream &o, Identifier i ) { return o << i.name; }
inline std::ostream &operator<<( std::ostream &o, Constant c ) { return o << c.value; }
inline std::ostream &operator<<( std::ostream &o, Application ap ) {
    return o << *ap.lhs << " " << *ap.rhs;
}

inline std::ostream &operator<<( std::ostream &o, Lambda l ) {
    return o << "(|" << l.bind.name() << "| " << *l.body << ")";
}

inline std::ostream &operator<<( std::ostream &o, BinOp op ) {
    return o << "(" << *op.lhs << " " << Token::tokenName[ int( op.op ) ] << " " << *op.rhs << ")";
}

inline std::ostream &operator<<( std::ostream &o, Expression e )
{
    if ( e.e.apply( PrintTo( o ) ).isNothing() )
        o << "<unknown expression>";
    return o;
}

inline Lambda::Lambda( Context &c ) : Parser( c ) {
    eat( TI::LambdaParen );
    bind = Pattern( c );
    eat( TI::LambdaParen );
    body = std::make_shared< Expression >( c );
}

inline IfThenElse::IfThenElse( Context &c ) : Parser( c ) {
    eat( TI::If );
    cond = std::make_shared< Expression >( c );
    eat( TI::Then );
    yes = std::make_shared< Expression >( c );
    eat( TI::Else );
    no = std::make_shared< Expression >( c );
}

struct Binding : Parser {
    Identifier name;
    Expression value;

    Binding() = default;
    Binding( Context &c ) : Parser( c ) {
        name = Identifier( c );
        eat( TI::Bind );
        value = Expression( c );
    }
};

struct Scope : Parser {
    std::vector< Binding > bindings;

    void separator() {
        Token t = eat( true );

        if ( t.id == TI::Newline )
            return separator(); /* eat all of them */
        if ( t.id == TI::Semicolon )
            return separator(); /* eat all of them */

        rewind( 1 ); /* done */
    }

    Scope() = default;
    Scope( Context &c ) : Parser( c ) {
        list< Binding >( std::back_inserter( bindings ), &Scope::separator );
    }
};

}

}
}

namespace divine_test {
namespace silk {

using namespace divine::silk::parse;

struct Parse {

    template< typename T >
    static T _parse( std::string s ) {
        Buffer b( s );
        Lexer< Buffer > lexer( b );
        Parser::Context ctx( lexer, "<inline>" );
        try {
            auto r = T( ctx );
            ASSERT( lexer.eof() );
            return r;
        } catch (...) {
            // ctx.errors( std::cerr );
            throw;
        }
    }

    TEST(scope) {
        auto c = _parse< Scope >( "a = 2 + 3" );
        ASSERT_EQ( int(c.bindings.size()), 1 );
        auto b = c.bindings[0];
        ASSERT_EQ( b.name.name, "a" );
        auto v = b.value.e;

        auto op = v.get< BinOp >();
        ASSERT( op.op == TI::Plus );
        Constant cnst( op.lhs->e );
        ASSERT_EQ( cnst.value, 2 );
        cnst = Constant( op.rhs->e );
        ASSERT_EQ( cnst.value, 3 );
    }

    TEST(scope2) {
        auto c = _parse< Scope >( "a = 2 + 3\nb = 4" );
        ASSERT_EQ( int ( c.bindings.size() ), 2 );
        auto b1 = c.bindings[0];
        ASSERT_EQ( b1.name.name, "a" );
        auto b2 = c.bindings[1];
        ASSERT_EQ( b2.name.name, "b" );
        auto cn = b2.value.e.get< Constant >();
        ASSERT_EQ( cn.value, 4 );
    }

    TEST(scope3) {
        auto c = _parse< Scope >( "a = 2 + 3; b = 4" );
        ASSERT_EQ( int ( c.bindings.size() ), 2 );
        auto b1 = c.bindings[0];
        ASSERT_EQ( b1.name.name, "a" );
        auto b2 = c.bindings[1];
        ASSERT_EQ( b2.name.name, "b" );
    }

    TEST(scope_expr) {
        SubScope c( _parse< Expression >( "{ a = 2 + 3; b = 4 }.b" ).e );
        Constant s( c.lhs->e );
        Identifier id( c.rhs->e );
    }

    TEST(scope_block) {
        Constant c( _parse< Expression >( "def a = 2 + 3; b = 4 end" ).e );
        ASSERT( c.scope );
    }

    TEST(simple) {
        BinOp op( _parse< Expression >( "x + 3" ).e );
        ASSERT( op.op == TI::Plus );
        Identifier id( op.lhs->e );
        Constant cnst( op.rhs->e );
        ASSERT_EQ( id.name, "x" );
        ASSERT_EQ( cnst.value, 3 );
    }

    TEST(lambda) {
        Lambda l( _parse< Expression >( "|x| x + 3" ).e );
        ASSERT_EQ( l.bind.name(), "x" );
        BinOp op( l.body->e );
        ASSERT( op.op == TI::Plus );
        Identifier id( op.lhs->e );
        Constant cnst( op.rhs->e );
        ASSERT_EQ( id.name, "x" );
        ASSERT_EQ( cnst.value, 3 );
    }

    TEST(nestedLambda) {
        Lambda l1( _parse< Expression >( "|x| |y| x + y" ).e );
        ASSERT_EQ( l1.bind.name(), "x" );

        Lambda l2( l1.body->e );
        ASSERT_EQ( l2.bind.name(), "y" );

        BinOp op( l2.body->e );
        Identifier id1( op.lhs->e );
        Identifier id2( op.rhs->e );
        ASSERT_EQ( id1.name, "x" );
        ASSERT_EQ( id2.name, "y" );
    }

    TEST(ifThenElse) {
        IfThenElse i( _parse< Expression >( "if x then x + y else z + y" ).e );
        ASSERT( i.cond->e.is< Identifier >() );
        ASSERT( i.yes->e.is< BinOp >() );
        ASSERT( i.no->e.is< BinOp >() );
    }

    TEST(complex) {
        auto i = _parse< Expression >( "if (|x| 3) then (if x + y then |a| 4 else |a| a) else |z| z + y" );
        auto top = IfThenElse( i.e );
        auto cond = Lambda( top.cond->e );
        auto yes = IfThenElse( top.yes->e );
        auto no = Lambda( top.no->e );
        auto op = BinOp( yes.cond->e );
        ASSERT( op.op == TI::Plus );
    }

    TEST(associativity) {
        auto top = BinOp( _parse< Expression >( "1 + 2 + 3" ).e );
        auto left = BinOp( top.lhs->e );
        auto right = Constant( top.rhs->e );
    }

    TEST(subscope1) {
        auto top = BinOp( _parse< Expression >( "1 + x.x" ).e );
        auto left = Constant( top.lhs->e );
        auto right = SubScope( top.rhs->e );
    }

    TEST(subscope2) {
        auto top = BinOp( _parse< Expression >( "x.x + 1" ).e );
        auto left = SubScope( top.lhs->e );
        auto right = Constant( top.rhs->e );
    }

    TEST(application) {
        auto top = Application( _parse< Expression >( "a b c" ).e );
        auto left = Application( top.lhs->e );
        auto a = Identifier( left.lhs->e );
        auto b = Identifier( left.rhs->e );
        auto c = Identifier( top.rhs->e );
        ASSERT_EQ( a.name, "a" );
        ASSERT_EQ( b.name, "b" );
        ASSERT_EQ( c.name, "c" );
    }

    TEST(compound) {
        auto top = Application( _parse< Expression >( "a (b + 1) c" ).e );
        auto left = Application( top.lhs->e );
        auto a = Identifier( left.lhs->e );
        auto b = BinOp( left.rhs->e );
        auto c = Identifier( top.rhs->e );
        ASSERT_EQ( a.name, "a" );
        ASSERT( b.op == TI::Plus );
        ASSERT_EQ( c.name, "c" );
    }

    TEST(print) {
        std::stringstream s;
        auto top = _parse< Expression >( "a + 3" );
        s << top;
        ASSERT_EQ( s.str(), "(a + 3)" );
    }

    TEST_FAILING(bad1) {
        _parse< Expression >( "a |x| a c" );
    }

    TEST(type) {
        Lambda( _parse< Expression >( "|x : _t| x" ).e );
    }

    TEST(type_fun) {
        Lambda( _parse< Expression >( "|x : _t -> _t| x" ).e );
    }

    TEST(type_pred) {
        Lambda( _parse< Expression >( "|x : fun? _t| x" ).e );
    }
};

}
}

#endif
