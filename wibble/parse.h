// -*- C++ -*- (c) 2011 Petr Rockai
// Support code for writing backtracking recursive-descent parsers
#include <string>
#include <cassert>
#include <deque>
#include <vector>

#include <wibble/regexp.h>

#ifndef WIBBLE_PARSE_H
#define WIBBLE_PARSE_H

namespace wibble {

struct Position {
    std::string source;
    int line;
    int column;
    Position() : source("-"), line(1), column(1) {}
    bool operator==( const Position &o ) const {
        return o.source == source && o.line == line && o.column == column;
    }
};

template< typename _Id >
struct Token {
    typedef _Id Id;
    Id id;
    std::string data;
    Position position;
    bool _valid;

    bool valid() { return _valid; }

    Token( Id _id, char c ) : id( _id ), _valid( true ) {
        data.push_back( c );
    }

    Token( Id _id, std::string d ) : id( _id ), data( d ), _valid( true ) {}
    Token() : id( Id( 0 ) ), _valid( false ) {}

    bool operator==( const Token &o ) const {
        return o._valid == _valid && o.id == id && o.data == data && o.position == position;
    }

};

template< typename Token, typename Stream >
struct Lexer {
    Stream &stream;
    std::string _window;
    Position current;

    void shift() {
        assert( !stream.eof() );
        char c = stream.remove();
        _window = _window + c; // XXX inefficient
    }

    bool eof() {
        return _window.empty() && stream.eof();
    }

    std::string window( unsigned n ) {
        while ( _window.length() < n && !stream.eof() )
            shift();
        return std::string( _window, 0, n );
    }

    void consume( int n ) {
        for( int i = 0; i < n; ++i ) {
            if ( _window[i] == '\n' ) {
                current.line ++;
                current.column = 1;
            } else
                current.column ++;
        }
        _window = std::string( _window, n, _window.length() );
    }

    void consume( std::string s ) {
        consume( s.length() );
    }

    void consume( Token t ) {
        consume( t.data );
    }

    bool maybe( Token &t, std::string data, typename Token::Id id ) {
        if ( t.valid() ) /* already found */
            return false;

        if ( window( data.length() ) == data ) {
            t = Token( id, data );
            consume( t );
            return true;
        }
        return false;
    }

    bool maybe( Token &t, Regexp r, typename Token::Id id ) {
        if ( t.valid() )
            return false;

        unsigned n = 1;
        if ( !r.match( window( 1 ) ) )
            return false;

        while ( r.match( window( n ) ) && r.matchLength( 0 ) == n && !stream.eof() )
            ++n;

        t = Token( id, window( n - 1 ) );
        consume( t );

        return true;
    }

    void continueTo( Token &t, std::string marker ) {
        while ( true ) {
            std::string _marker = window( marker.length() );
            if ( _marker == marker || stream.eof() ) {
                t.data += _marker;
                consume( _marker );
                return;
            } else {
                t.data += window( 1 );
                consume( 1 );
            }
        }
    }

    void skipWhitespace() {
        while ( !eof() && isspace( window( 1 )[ 0 ] ) )
            consume( 1 );
    }

    Lexer( Stream &s ) : stream( s ) {}
};

template< typename Token, typename Stream >
struct ParseContext {
    Stream &stream;
    std::deque< Token > done, todo;
    int position;

    struct Fail {
        int position;
        std::string expected;
        Token token;

        Fail( ParseContext &ctx, std::string err ) {
            token = ctx.remove();
            ctx.rewind( 1 );
            expected = err;
        }
        ~Fail() throw () {}
    };

    std::vector< Fail > failures;
    int last_fork;
    int maybe_nesting;

    void error( std::ostream &o, std::string prefix, const Fail &fail ) {
        o << prefix
          << "expected " << fail.expected
          << " at line " << fail.token.position.line
          << ", column " << fail.token.position.column
          << ", but seen " << Token::tokenName[ fail.token.id ] << " '" << fail.token.data << "'"
          << std::endl;
    }

    void errors( std::ostream &o ) {
        purge_failures();
        if ( failures.empty() ) {
            o << "oops, no failures recorded!" << std::endl;
            return;
        }

        if ( failures.size() > 1 ) {
            o << "parse error: " << failures.size() << " alternatives failed:" << std::endl;
            for ( unsigned i = 0; i < failures.size(); ++i )
                error( o, "    ", failures[i] );
        } else
            error( o, "", failures.front() );
    }

    Token remove() {
        Token t;
        if ( !todo.empty() ) {
            t = todo.front();
            todo.pop_front();
        } else {
            do {
                t = stream.remove();
            } while ( t.id == Token::Comment );
        }

        done.push_back( t );
        ++ position;

        return t;
    }

    void rewind( int n ) {
        assert( n >= 0 );
        for ( int i = 0; i < n; ++i ) {
            todo.push_front( done.back() );
            done.pop_back();
            -- position;
        }
    }

    int start_maybe() {
        if ( !maybe_nesting )
            last_fork = position;
        ++ maybe_nesting;
        return position;
    }

    void end_maybe( int fallback ) {
        -- maybe_nesting;
        if ( fallback != last_fork )
            return;

        purge_failures();
    }

    void purge_failures() {
        for ( unsigned i = 0; i < failures.size(); ++i ) {
            if ( failures[i].position < last_fork ) {
                failures.erase( failures.begin() + i );
                -- i;
            }
        }
    }

    void fail_maybe() {
        -- maybe_nesting;
    }

    ParseContext( Stream &s ) : stream( s ), position( 0 ), maybe_nesting( 0 ) {}
};

template< typename Token, typename Stream >
struct Parser {
    typedef typename Token::Id TokenId;
    typedef ParseContext< Token, Stream > Context;
    Context *ctx;
    typedef typename Context::Fail Fail;

    bool valid() const {
        return ctx;
    }

    Context &context() {
        assert( ctx );
        return *ctx;
    }

    int position() {
        return context().position;
    }

    void fail( std::string what ) __attribute__((noreturn))
    {
        throw Fail( context(), what );
    }

    void semicolon() {
        Token t = eat();
        if ( t.id == Token::Punctuation && t.data == ";" )
            return;

        context().rewind( 1 );
        fail( "semicolon" );
    }

    Token eat( TokenId id ) {
        Token t = eat();
        if ( t.id == id )
            return t;
        context().rewind( 1 );
        fail( std::string( "#" ) + Token::tokenName[id] );
    }

    template< typename F, typename G >
    void maybe( F f, G g ) {
        if ( maybe( f ) )
            return;
        maybe( g );
    }

    template< typename F, typename G >
    void either( F f, void (G::*g)() ) {
        if ( maybe( f ) )
            return;
        (static_cast< G* >( this )->*g)();
    }

    template< typename F, typename G, typename H >
    void maybe( F f, G g, H h ) {
        if ( maybe( f ) )
            return;
        if ( maybe( g ) )
            return;
        maybe( h );
    }

    template< typename F >
    bool maybe( void (F::*f)() ) {
        int fallback = context().start_maybe();
        try {
            (static_cast< F* >( this )->*f)();
            context().end_maybe( fallback );
            return true;
        } catch ( Fail fail ) {
            fail.position = fallback;
            context().fail_maybe();
            context().rewind( position() - fallback );
            context().failures.push_back( fail );
            return false;
        }
    }

    template< typename T >
    std::vector< T > many() {
        int fallback;
        std::vector< T > vec;
        while ( true ) {
            try {
                fallback = position();
                vec.push_back( T( context() ) );
            } catch (Fail) {
                context().rewind( position() - fallback );
                return vec;
            }
        }
    }

    template< typename T >
    std::vector< T > list( TokenId sep ) {
        std::vector< T > vec;
        while ( true ) {
            vec.push_back( T( context() ) );
            if ( !next( sep ) )
                return vec;
        }
    }

    template< typename T >
    std::vector< T > list( TokenId first, TokenId sep, TokenId last ) {
        eat( first );
        std::vector< T > res = list< T >( sep );
        eat( last );
        return res;
    }

    Token eat() {
        return context().remove();
    }

    bool next( TokenId id ) {
        Token t = eat();
        if ( t.id == id )
            return true;
        context().rewind( 1 );
        return false;
    }

    Parser( Context &c ) : ctx( &c ) {}
    Parser() : ctx( 0 ) {}

};

}

#endif
