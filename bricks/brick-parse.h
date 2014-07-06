// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * A brick for writing backtracking, recursive-descent parsers. This is plain
 * C++98 with some exceptions (guarded by #ifdefs). The goal here is to make it
 * easy to write parsers in C++ -- anything based on this code is not going to
 * be very fast. If you are after speed, look at parser generators or at boost.
 */

/*
 * (c) 2013-2014 Petr Ročkai <me@mornfall.net>
 * (c) 2013 Jan Kriho
 */

/* Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. */

#include <brick-string.h>
#include <brick-unittest.h>

#include <string>
#include <deque>
#include <vector>
#include <map>
#include <queue>

#ifndef BRICK_PARSE_H
#define BRICK_PARSE_H

namespace brick {
namespace parse {

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
        bool valid = ensure_window( n );
        assert( valid );
        static_cast< void >( valid );
        std::deque< char >::iterator b = _window.begin(), e = b;
        e += n;
        return std::string( b, e );
    }

    bool ensure_window( unsigned n ) {
        while ( _window.size() < n && !stream.eof() )
            shift();
        return _window.size() >= n;
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
        if ( !ensure_window( end - begin ) )
            return false;
        return std::equal( begin, end, _window.begin() );
    }

    void match( const std::string &data, typename Token::Id id ) {
        if ( match( data.begin(), data.end() ) )
            return keep( id, data );
    }

#if 0
    void match( Regexp &r, typename Token::Id id ) {
        unsigned n = 1, max = 0;
        while ( r.match( window( n ) ) ) {
            if ( max && max == r.matchLength( 0 ) )
                return keep( id, window( max ) );
            max = r.matchLength( 0 );
            ++ n;
        }
    }
#endif

    void match( int (*first)(int), int (*rest)(int), typename Token::Id id )
    {
        unsigned n = 1;

        if ( !ensure_window( 1 ) )
            return;

        if ( !first( _window[0] ) )
            return;

        while ( true ) {
            ++ n;
            if ( ensure_window( n ) && rest( _window[ n - 1 ] ) )
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
            if ( !ensure_window( n + to.length() ) )
                return;

            if ( std::equal( to.begin(), to.end(), where ) )
                return keep( id, window( n + to.length() ) );
            ++ where;
            ++ n;
        }
    }

    void skipWhile( int (*pred)(int) ) {
        while ( !eof() && pred( window( 1 )[ 0 ] ) )
            consume( 1 );
    }

    void skipWhitespace() { skipWhile( isspace ); }

    Token decide() {
        Token t;
        std::swap( t, _match );
        consume( t );
        return t;
    }

    Lexer( Stream &s ) : stream( s ) {}
};

template< typename Token, typename Stream >
struct ParseContext
{
    Stream *_stream;
    Stream &stream() { assert( _stream ); return *_stream; }

    std::deque< Token > window;
    int window_pos;
    int position;
    std::string name;
    typedef ParseContext< Token, Stream > This;
    std::vector< This > children;

    struct Fail {
        enum Type { Syntax, Semantic };

        int position;
        const char *expected;
        Type type;

        bool operator<( const Fail &other ) const {
            return position > other.position;
        }

        Fail( const char *err, int pos, Type t = Syntax) {
            expected = err;
            position = pos;
            type = t;
        }
        ~Fail() throw () {}
    };

    std::priority_queue< Fail > failures;

    void clearErrors() {
        failures = std::priority_queue< Fail >();
    }

    void error( std::ostream &o, std::string prefix, const Fail &fail ) {
        Token t = window[ fail.position ];
        switch ( fail.type ) {
            case Fail::Syntax:
                o << prefix
                << "expected " << fail.expected
                << " at line " << t.position.line
                << ", column " << t.position.column
                << ", but seen " << Token::tokenName[ static_cast< int >( t.id ) ] << " '" << t.data << "'"
                << std::endl;
                return;
            case Fail::Semantic:
                o << prefix
                << fail.expected
                << " at line " << t.position.line
                << ", column " << t.position.column
                << std::endl;
                return;
        }
    }

    void errors( std::ostream &o ) {
        for ( This & pc : children )
            pc.errors( o );

        if ( failures.empty() )
            return;

        std::string prefix;
        switch ( failures.top().type ) {
            case Fail::Syntax:
                o << "parse";
                break;
            case Fail::Semantic:
                o << "semantic";
                break;
        }
        o << " error in context " << name << ": ";
        if ( failures.size() > 1 ) {
            o << failures.size() << " rightmost alternatives:" << std::endl;
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
                t = stream().remove();
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

    This & createChild( Stream &s, std::string name ) {
        children.push_back( This( s, name ) );
        return children.back();
    }

    ParseContext( Stream &s, std::string name )
        : _stream( &s ), window_pos( 0 ), position( 0 ), name( name )
    {}
};

template< typename Token, typename Stream >
struct Parser {
    typedef typename Token::Id TokenId;
    typedef ParseContext< Token, Stream > Context;
    Context *ctx;
    typedef typename Context::Fail Fail;
    typedef typename Fail::Type FailType;
    int _position;

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

    void rewind( int i ) {
        context().rewind( i );
        _position = context().position;
    }

    void fail( const char *what, FailType type = FailType::Syntax ) __attribute__((noreturn))
    {
        Fail f( what, _position, type );
        context().failures.push( f );
        while ( context().failures.top().position < _position )
            context().failures.pop();
        throw f;
    }

    void semicolon() {
        Token t = eat();
        if ( t.id == Token::Punctuation && t.data == ";" )
            return;

        rewind( 1 );
        fail( "semicolon" );
    }

    void colon() {
        Token t = eat();
        if ( t.id == Token::Punctuation && t.data == ":" )
            return;

        rewind( 1 );
        fail( "colon" );
    }

    Token eat( TokenId id ) {
        Token t = eat( false );
        if ( t.id == id )
            return t;
        rewind( 1 );
        fail( Token::tokenName[ static_cast< int >( id ) ].c_str() );
    }


#if __cplusplus >= 201103L
    template< typename F >
    void either( void (F::*f)() ) {
        (static_cast< F* >( this )->*f)();
    }

    template< typename F, typename... Args >
    void either( F f, Args... args ) {
        if ( maybe( f ) )
            return;
        either( args... );
    }
#else
    template< typename F, typename G >
    void either( F f, void (G::*g)() ) {
        if ( maybe( f ) )
            return;
        (static_cast< G* >( this )->*g)();
    }
#endif


#if __cplusplus >= 201103L
    template< typename F, typename... Args >
    bool maybe( F f, Args... args ) {
        if ( maybe( f ) )
            return true;
        return maybe( args... );
    }

#else
    template< typename F, typename G >
    bool maybe( F f, G g ) {
        if ( maybe( f ) )
            return true;
        return maybe( g );
    }


    template< typename F, typename G, typename H >
    bool maybe( F f, G g, H h ) {
        if ( maybe( f ) )
            return true;
        if ( maybe( g ) )
            return true;
        return maybe( h );
    }

#endif

    template< typename F >
    bool maybe( void (F::*f)() ) {
        int fallback = position();
        try {
            (static_cast< F* >( this )->*f)();
            return true;
        } catch ( Fail fail ) {
            rewind( position() - fallback );
            return false;
        }
    }

#if __cplusplus >= 201103L
    template< typename F >
    auto maybe( F f ) -> decltype( f(), true ) {
        int fallback = position();
        try {
            f(); return true;
        } catch ( Fail fail ) {
            rewind( position() - fallback );
            return false;
        }
    }
#endif

    bool maybe( TokenId id ) {
        int fallback = position();
        try {
            eat( id );
            return true;
        } catch (Fail) {
            rewind( position() - fallback );
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
            rewind( position() - fallback );
        }
    }
#if __cplusplus >= 201103L
    template< typename F >
    bool arbitrary( F f ) {
        return maybe( f );
    }

    // NB. Accepts any ordering of sub-parsers, some possibly more than
    // once. It is slightly bogus and won't catch all errors in most reasonable
    // languages. Avoid.
    template< typename F, typename... Args >
    bool arbitrary( F f, Args... args ) {
        bool retval = arbitrary( args... );
        retval |= maybe( f );
        retval |= arbitrary( args... );
        return retval;
    }
#endif

    template< typename T, typename I >
    void list( I i, TokenId sep ) {
        do {
            *i++ = T( context() );
        } while ( next( sep ) );
    }

    template< typename T, typename I, typename F >
    void list( I i, void (F::*sep)() ) {
        int fallback = position();
        try {
            while ( true ) {
                *i++ = T( context() );
                fallback = position();
                (static_cast< F* >( this )->*sep)();
            }
        } catch(Fail) {
            rewind( position() - fallback );
        }
    }

    template< typename T, typename I >
    void list( I i, TokenId first, TokenId sep, TokenId last ) {
        eat( first );
        list< T >( i, sep );
        eat( last );
    }

    Token eat( bool _fail = true ) {
        Token t = context().remove();
        _position = context().position;
        if ( _fail && !t.valid() ) {
            rewind( 1 );
            fail( "valid token" );
        }
        return t;
    }

    bool next( TokenId id ) {
        Token t = eat( false );
        if ( t.id == id )
            return true;
        rewind( 1 );
        return false;
    }

    Parser( Context &c ) : ctx( &c ) {}
    Parser() : ctx( 0 ) {}

};

}
}

namespace brick_test {
namespace parse {

using namespace ::brick::parse;

struct Parse {
    enum TokenId { Invalid, Number };
    typedef brick::parse::Token< TokenId > Token;

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

    struct _Lexer : Lexer< Token, IOStream >
    {
        Token remove() {
            this->skipWhitespace();
            this->match( isdigit, isdigit, Number );
            return this->decide();
        }

        _Lexer( IOStream &s )
            : Lexer< Token, IOStream >( s )
        {}
    };

    TEST(lexer) {
        std::stringstream s;
        IOStream is( s );
        Token t;
        _Lexer l( is );

        s << "1 2";

        t = l.remove();
        ASSERT_EQ( t.id, Number );
        ASSERT_EQ( t.data, "1" );

        t = l.remove();
        ASSERT_EQ( t.id, Number );
        ASSERT_EQ( t.data, "2" );

        t = l.remove();
        ASSERT_EQ( t.id, Invalid );
    }
};

}
}

#endif
// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab
