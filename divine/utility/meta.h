// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>
#include <wibble/test.h> // for assert
#include <iostream>
#include <fstream>
#include <divine/utility/sysinfo.h>
#include <stdint.h>

#ifndef DIVINE_META_H
#define DIVINE_META_H

namespace divine {
namespace meta {

struct Input {
    enum PT { NoProperty, LTL, Neverclaim, Reachability };
    std::string modelType;
    std::string model;
    std::string property;
    std::string propertyName;
    std::string trace;
    PT propertyType;
    bool dummygen;

    Input() : propertyType( NoProperty ), dummygen( false ) {}
};

struct Execution {
    int initialTable;
    int diskFifo;

    int threads;
    int nodes;
    int thisNode;

    Execution() : initialTable( 32 ), diskFifo( 0 ), threads( 2 ), nodes( 1 ), thisNode( 0 ) {}
};

struct Algorithm {
    enum Type { Metrics, Reachability, Ndfs, Map, Owcty, Verify,
                Draw, Info, Compact, Probabilistic };
    enum Reduction { POR, Tau, TauPlus, TauStores, Heap };

    Type algorithm;
    std::string name;
    uint32_t hashSeed;

    int maxDistance;

    bool labels, traceLabels, bfsLayout; /* for drawing */
    bool findDeadlocks;
    bool findGoals;
    bool hashCompaction;
    bool onlyQualitative, iterativeOptimization;
    bool fairness;
    std::set< Reduction > reduce;

    Algorithm() : algorithm( Info ),
                  hashSeed( 0 ),
                  maxDistance( 32 ),
                  labels( false ), traceLabels( false ), bfsLayout( false ),
                  findDeadlocks( true ), findGoals( true ),
                  hashCompaction( false ), onlyQualitative( false ), iterativeOptimization( true ),
                  fairness( false ) {}
};

struct Output {
    bool quiet;
    bool wantCe;
    bool textFormat, backEdges;
    bool statistics;
    std::string filterProgram;
    std::string trailFile, ceFile;
    std::string file;

    Output() : quiet( false ), wantCe( false ), textFormat( false ), backEdges( false ), statistics( false ) {}
};

struct Statistics {
    int64_t visited, expanded, deadlocks, transitions, accepting;
    Statistics() : visited( 0 ), expanded( 0 ), deadlocks( 0 ), transitions( 0 ), accepting( 0 ) {}
};

struct Result
{
    enum R { Yes, No, Unknown };
    enum CET { NoCE, Cycle, Deadlock, Goal };
    R propertyHolds, fullyExplored;
    CET ceType;
    std::string iniTrail, cycleTrail;
    Result() :
        propertyHolds( Unknown ), fullyExplored( Unknown ),
        ceType( NoCE ), iniTrail( "-" ), cycleTrail( "-" )
    {}
};

}

std::ostream &operator<<( std::ostream &o, meta::Result::R v );
std::ostream &operator<<( std::ostream &o, meta::Result::CET v );
std::ostream &operator<<( std::ostream &o, meta::Input::PT v );
std::ostream &operator<<( std::ostream &o, meta::Input i );
std::ostream &operator<<( std::ostream &o, meta::Result r );
std::ostream &operator<<( std::ostream &o, meta::Algorithm a );
std::ostream &operator<<( std::ostream &o, meta::Execution a );
std::ostream &operator<<( std::ostream &o, meta::Output a );
std::ostream &operator<<( std::ostream &o, meta::Statistics a );

struct Meta {
    meta::Input input;
    meta::Algorithm algorithm;
    meta::Execution execution;
    meta::Output output;
    meta::Result result;
    meta::Statistics statistics;
};

std::ostream &operator<<( std::ostream &o, Meta m );

}

#endif
