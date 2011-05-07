// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>
#include <wibble/sys/thread.h>
#include <wibble/test.h> // for assert
#include <iostream>
#include <fstream>
#include <divine/datastruct.h>

#ifndef DIVINE_CONFIG_H
#define DIVINE_CONFIG_H

namespace divine {

// configuration stuff, may need refactoring later
struct Config {
    int workers;
    int initialTable;
    bool wantCe;

    // for reachability
    bool findDeadlocks;
    bool findGoals;

    std::string input;
    std::string trailFile, ceFile;
    std::string property;

    // compact options
    std::string compactFile;
    bool findBackEdges, textFormat; //keepOriginal;

    // probabilistic options
    bool onlyQualitative, iterativeOptimization;

    std::ostream &dump( std::ostream &o ) {
        o << "Input-File: " << input << std::endl;
        o << "Trail-File: " << trailFile << std::endl;
        o << "Workers: " << workers << std::endl;
        return o;
    }

    Config() : workers( 2 ), initialTable( 4096 ), wantCe( true ) {}
};

}

#endif
