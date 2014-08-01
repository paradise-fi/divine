// -*- C++ -*- (c) 2011 Petr Rockai

#include <cassert>
#include <wibble/parse.h>

#ifndef DIVINE_DVE_LEX_H
#define DIVINE_DVE_LEX_H

namespace divine {
namespace dve {

struct TI {
    enum TokenId {
        Invalid,
        // the following 2 IDs are internal to the expression evaluator, which
        // reuses TokenId for representing operations
        UnaryMinus, Subscript, Reference,
        Comment, Punctuation, Identifier, Constant, String,
        IndexOpen, IndexClose, BlockOpen, BlockClose, ParenOpen, ParenClose,
        Input, Expression, Key, For, If, Else,
        Const, Int, Byte, Channel,
        Bool_Or, Bool_And, Bool_Not, Imply,
        LEq, Lt, GEq, Gt, Eq, NEq,
        Comma, Period, Ellipsis, SyncWrite, SyncRead, Tilde, Arrow, Wide_Arrow, Assignment,
        Plus, Minus, Mult, Div, Mod, Or, And, Xor, LShift, RShift,
        Increment, Decrement,
        Init, State, Trans, Accept,
        Guard, Effect,
        Assert, Commit,
        System, Async, Sync, Process, Property, LTL,
        Streett, Muller, Rabin, Buchi, GenBuchi
    };
};

const std::string tokenName[] = {
    "INVALID", "MINUS", "SUBSCRIPT", "REFERENCE",
    "comment", "punctuation", "identifier", "constant", "string",
    "[", "]", "{", "}", "(", ")",
    "input", "expression", "key", "for", "if", "else",
    "const", "int", "byte", "channel",
    "||", "&&", "not", "imply",
    "<=", "<", ">=", ">", "==", "!=",
    ",", ".", "..", "!", "?", "~", "->", "=>", "=",
    "+", "-", "*", "/", "%", "|", "&", "^", "<<", ">>",
    "++", "--",
    "init", "state", "trans", "accept",
    "guard", "effect",
    "assert", "commit",
    "system", "async", "sync", "process", "property", "ltl",
    "streett", "muller", "rabin", "buchi", "genbuchi"
};

struct Token : wibble::Token< TI::TokenId >, TI {
    Token( Id id, char c ) : wibble::Token< TI::TokenId >( id, c ) {}
    Token( Id id, std::string c ) : wibble::Token< TI::TokenId >( id, c ) {}
    Token() : wibble::Token< TI::TokenId >() {}

    static const std::string *tokenName;
    static const int precedences = 6;

    bool precedence( int p ) const {
        if ( p == 1 )
            return id == Period || id == Arrow;
        if ( p == 2 )
            return id == And || id == Or || id == Xor || id == LShift || id == RShift;
        if ( p == 3 )
            return id == Mult || id == Div || id == Mod;
        if ( p == 4 )
            return id == Plus || id == Minus;
        if ( p == 5 )
            return id == Eq || id == NEq || id == LEq || id == Lt || id == Gt || id == GEq;
        if ( p == 6 )
            return id == Bool_And || id == Bool_Or || id == Imply;
        return false;
    }

    bool unary() const {
        return id == Bool_Not || id == Tilde || id == UnaryMinus;
    }
};

const std::string __fragments[] = {
    "/*", "*/", "//", "\n", ";", ":", "and", "or", "true", "false", "'","\""
};

#undef TRUE
#undef FALSE

struct Fragment {
    enum Frag { LC, EC, SC, EOL, SEMICOL, COL, AND, OR, TRUE, FALSE, SQ, DQ };
};

template< typename Stream >
struct Lexer : wibble::Lexer< Token, Stream >, Fragment {

    static int firstident( int c ) {
        return isalpha( c ) || c == '_';
    }

    static int restident( int c ) {
        return isalnum( c ) || c == '_';
    }

    const std::string &frag( Frag f ) { return __fragments[f]; }

    Token remove() {
        this->skipWhitespace();

        this->match( frag( LC ), frag( EC ), Token::Comment );
        this->match( frag( SC ), frag( EOL ), Token::Comment );

        this->match( frag( SQ ), frag( SQ ), Token::String );
        this->match( frag( DQ ), frag( DQ ), Token::String );

        this->match( frag( TRUE ), TI::Constant );
        this->match( frag( FALSE ), TI::Constant );

        for ( TI::TokenId i = TI::IndexOpen; i < TI::GenBuchi; i = static_cast<TI::TokenId>(i + 1) )
            this->match( tokenName[i], i );

        this->match( frag( SEMICOL ), Token::Punctuation );
        this->match( frag( COL ), Token::Punctuation );

        this->match( frag( OR ), Token::Bool_Or );
        this->match( frag( AND ), Token::Bool_And );

        this->match( firstident, restident, TI::Identifier );
        this->match( isdigit, isdigit, TI::Constant );

        return this->decide();
    }

    Lexer( Stream &s )
        : wibble::Lexer< Token, Stream >( s )
    {}
};

static inline std::ostream &operator<<( std::ostream &o, Token t ) {
    return o << "<" << t.position.source << ":" << t.position.line << ":" << t.position.column << ": #"
             << tokenName[ t.id ] << ", '" << t.data << "'>";
}


}
}

#endif
