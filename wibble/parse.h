// -*- C++ -*- (c) 2011 Petr Rockai
// Support code for writing backtracking recursive-descent parsers
#include <string>
#include <cassert>
#include <deque>
#include <vector>
#include <map>
#include <queue>

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

template< typename X, typename Y >
inline std::ostream &operator<<( std::ostream &o, const std::pair< X, Y > &x ) {
    return o << "(" << x.first << ", " << x.second << ")";
}

/*
 * This is a SLOW lexer (at least compared to systems like lex/flex, which
 * build a finite-state machine to make decisions per input character. We could
 * do that in theory, but it would still be slow, since we would be in effect
 * interpreting the FSM anyway, while (f)lex uses an optimising compiler like
 * gcc. So while this is friendly and flexible, it won't give you a fast
 * scanner.
 */
template< typename Token, typename Stream >
struct Lexer {
    Stream &stream;
    typedef std::deque< char > Window;
    Window _window;
    Position current;

    Token _match;

    void shift() {
        assert( !stream.eof() );
        std::string r = stream.remove();
        std::copy( r.begin(), r.end(), std::back_inserter( _window ) );
    }

    bool eof() {
        return _window.empty() && stream.eof();
    }

    std::string window( unsigned n ) {
        ensure_window( n );
        std::deque< char >::iterator b = _window.begin(), e = b;
        e += n;
        return std::string( b, e );
    }

    void ensure_window( unsigned n ) {
        while ( _window.size() < n && !stream.eof() )
            shift();
    }

    void consume( int n ) {
        for( int i = 0; i < n; ++i ) {
            if ( _window[i] == '\n' ) {
                current.line ++;
                current.column = 1;
            } else
                current.column ++;
        }
        std::deque< char >::iterator b = _window.begin(), e = b;
        e += n;
        _window.erase( b, e );
    }

    void consume( const std::string &s ) {
        consume( s.length() );
    }

    void consume( const Token &t ) {
        // std::cerr << "consuming " << t << std::endl;
        consume( t.data );
    }

    void keep( typename Token::Id id, const std::string &data ) {
        Token t( id, data );
        t.position = current;
        if ( t.data.length() > _match.data.length() )
            _match = t;
    }

    template< typename I >
    bool match( I begin, I end ) {
        ensure_window( end - begin);
        return std::equal( begin, end, _window.begin() );
    }

    void match( const std::string &data, typename Token::Id id ) {
        if ( match( data.begin(), data.end() ) )
            return keep( id, data );
    }

    void match( Regexp &r, typename Token::Id id ) {
        unsigned n = 1, max = 0;
        while ( r.match( window( n ) ) ) {
            if ( max && max == r.matchLength( 0 ) )
                return keep( id, window( max ) );
            max = r.matchLength( 0 );
            ++ n;
        }
    }

    void match( int (*first)(int), int (*rest)(int), typename Token::Id id )
    {
        unsigned n = 1;

        ensure_window( 1 );
        if ( !first( _window[0] ) )
            return;

        while ( true ) {
            ++ n;
            ensure_window( n );
            if ( rest( _window[ n - 1 ] ) )
                continue;
            return keep( id, window( n - 1 ) );
        }
    }

    void match( const std::string &from, const std::string &to, typename Token::Id id ) {
        if ( !match( from.begin(), from.end() ) )
            return;

        Window::iterator where = _window.begin();
        int n = from.length();
        where += n;
        while ( true ) {
            ensure_window( n + to.length() );
            if ( std::equal( to.begin(), to.end(), where ) )
                return keep( id, window( n + to.length() ) );
            ++ where;
            ++ n;
        }
    }

    void skipWhitespace() {
        while ( !eof() && isspace( window( 1 )[ 0 ] ) )
            consume( 1 );
    }

    Token decide() {
        Token t;
        std::swap( t, _match );
        consume( t );
        return t;
    }

    Lexer( Stream &s ) : stream( s ) {}
};

template< typename Token, typename Stream >
struct ParseContext {
    Stream &stream;
    std::deque< Token > window;
    int window_pos;
    int position;

    struct Fail {
        int position;
        const char *expected;

        bool operator<( const Fail &other ) const {
            return position > other.position;
        }

        Fail( const char *err, int pos ) {
            expected = err;
            position = pos;
        }
        ~Fail() throw () {}
    };

    std::priority_queue< Fail > failures;

    void error( std::ostream &o, std::string prefix, const Fail &fail ) {
        Token t = window[ fail.position ];
        o << prefix
          << "expected " << fail.expected
          << " at line " << t.position.line
          << ", column " << t.position.column
          << ", but seen " << Token::tokenName[ t.id ] << " '" << t.data << "'"
          << std::endl;
    }

    void errors( std::ostream &o ) {
        if ( failures.empty() ) {
            o << "oops, no failures recorded!" << std::endl;
            return;
        }

        std::string prefix;
        if ( failures.size() > 1 ) {
            o << "parse error: " << failures.size() << " rightmost alternatives:" << std::endl;
            prefix = "    ";
        }
        while ( !failures.empty() ) {
            error( o, prefix, failures.top() );
            failures.pop();
        }
    }

    Token remove() {
        if ( int( window.size() ) <= window_pos ) {
            Token t;
            do {
                t = stream.remove();
            } while ( t.id == Token::Comment ); // XXX
            window.push_back( t );
        }

        ++ window_pos;
        ++ position;
        return window[ window_pos - 1 ];
    }

    void rewind( int n ) {
        assert( n >= 0 );
        assert( n <= window_pos );
        window_pos -= n;
        position -= n;
    }

    ParseContext( Stream &s ) : stream( s ), window_pos( 0 ), position( 0 ) {}
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

    void fail( const char *what ) __attribute__((noreturn))
    {
        Fail f( what, position() );
        context().failures.push( f );
        while ( context().failures.top().position < position() )
            context().failures.pop();
        throw f;
    }

    void semicolon() {
        Token t = eat();
        if ( t.id == Token::Punctuation && t.data == ";" )
            return;

        context().rewind( 1 );
        fail( "semicolon" );
    }

    Token eat( TokenId id ) {
        Token t = eat( false );
        if ( t.id == id )
            return t;
        context().rewind( 1 );
        fail( Token::tokenName[id].c_str() );
    }

    template< typename F, typename G >
    bool maybe( F f, G g ) {
        if ( maybe( f ) )
            return true;
        return maybe( g );
    }

    template< typename F, typename G >
    void either( F f, void (G::*g)() ) {
        if ( maybe( f ) )
            return;
        (static_cast< G* >( this )->*g)();
    }

    template< typename F, typename G, typename H >
    bool maybe( F f, G g, H h ) {
        if ( maybe( f ) )
            return true;
        if ( maybe( g ) )
            return true;
        return maybe( h );
    }

    template< typename F >
    bool maybe( void (F::*f)() ) {
        int fallback = position();
        try {
            (static_cast< F* >( this )->*f)();
            return true;
        } catch ( Fail fail ) {
            context().rewind( position() - fallback );
            return false;
        }
    }

    template< typename T, typename I >
    void many( I i ) {
        int fallback = 0;
        try {
            while ( true ) {
                fallback = position();
                *i++ = T( context() );
            }
        } catch (Fail) {
            context().rewind( position() - fallback );
        }
    }

    template< typename T, typename I >
    void list( I i, TokenId sep ) {
        do {
            *i++ = T( context() );
        } while ( next( sep ) );
    }

    template< typename T, typename I >
    void list( I i, TokenId first, TokenId sep, TokenId last ) {
        eat( first );
        list< T >( i, sep );
        eat( last );
    }

    Token eat( bool _fail = true ) {
        Token t = context().remove();
        if ( _fail && !t.valid() ) {
            context().rewind( 1 );
            fail( "valid token" );
        }
        return t;
    }

    bool next( TokenId id ) {
        Token t = eat( false );
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
