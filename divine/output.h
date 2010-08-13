// -*- C++ -*- Copyright (c) 2010 Petr Rockai <me@mornfall.net>

#ifndef DIVINE_OUTPUT_H
#define DIVINE_OUTPUT_H

namespace divine {

struct Output {
    virtual std::ostream &statistics() = 0;
    virtual std::ostream &progress() = 0;
    virtual std::ostream &debug() = 0;
    virtual void setStatsSize( int x, int y ) {}
    virtual void cleanup() {}

    static Output *_output;
    static Output &output() {
        assert( _output );
        return *_output;
    }
};

Output *makeStdIO( std::ostream & );
Output *makeCurses();

}

#endif
