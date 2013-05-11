// -*- C++ -*-

#include <wibble/parse.h>
#include <wibble/test.h>

using namespace wibble;


struct TestParse {
    enum TokenId { Invalid, Number };
    typedef wibble::Token< TokenId > Token;

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

    struct Lexer : wibble::Lexer< Token, IOStream >
    {
        Token remove() {
            this->skipWhitespace();
            this->match( isdigit, isdigit, Number );
            return this->decide();
        }

        Lexer( IOStream &s )
            : wibble::Lexer< Token, IOStream >( s )
        {}
    };

    Test lexer() {
        std::stringstream s;
        IOStream is( s );
        Token t;
        Lexer l( is );

        s << "1 2";

        t = l.remove();
        assert_eq( t.id, Number );
        assert_eq( t.data, "1" );

        t = l.remove();
        assert_eq( t.id, Number );
        assert_eq( t.data, "2" );

        t = l.remove();
        assert_eq( t.id, Invalid );
    }
};
