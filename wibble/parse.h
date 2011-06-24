// -*- C++ -*- (c) 2011 Petr Rockai
// Support code for writing backtracking recursive-descent parsers
#include <string>
#include <cassert>
#include <deque>
#include <vector>

#include <wibble/regexp.h>

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
    bool valid;

    Token( Id _id, char c ) : id( _id ), valid( true ) {
        data.push_back( c );
    }

    Token( Id _id, std::string d ) : id( _id ), data( d ), valid( true ) {}
    Token() : id( Id( 0 ) ), valid( false ) {}

    bool operator==( const Token &o ) const {
        return o.valid == valid && o.id == id && o.data == data && o.position == position;
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
        if ( t.valid ) /* already found */
            return false;

        if ( window( data.length() ) == data ) {
            t = Token( id, data );
            consume( t );
            return true;
        }
        return false;
    }

    bool maybe( Token &t, Regexp r, typename Token::Id id ) {
        if ( t.valid )
            return false;

        int n = 1;
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


}
