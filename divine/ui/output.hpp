// -*- C++ -*- Copyright (c) 2010 Petr Rockai <me@mornfall.net>

#include <memory>
#include <iostream>
#include <brick-unittest>

#ifndef DIVINE_OUTPUT_H
#define DIVINE_OUTPUT_H

namespace divine {

struct Output {
    virtual std::ostream &statistics() = 0;
    virtual std::ostream &progress() = 0;
    virtual std::ostream &debug() = 0;
    virtual void setStatsSize( int /* x */, int /* y */ ) {}
    virtual void cleanup() {}
    virtual ~Output() {}
    using Ptr = std::shared_ptr< Output >;

    struct Token {
        Token( Ptr h ) : _held( h ) {}
        bool check( Ptr p ) const { return p == _held; }
    private:
        Ptr _held;
    };

    static std::shared_ptr< Output > _output;

    static Token hold() {
        return Token( _output );
    }

    static Output &output() {
        ASSERT( !!_output );
        return *_output;
    }

    static Output &output( const Token &t ) {
        ASSERT( t.check( _output ) );
        return *_output;
    }
};

Output *makeStdIO( std::ostream & );
Output *makeCurses();

}

#endif
