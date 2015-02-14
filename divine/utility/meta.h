// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>
//             (c) 2013 Vladimír Štill <xstill@fi.muni.cz>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <iterator>
#include <initializer_list>

#include <divine/graph/graph.h>
#include <divine/utility/withreport.h>

#ifndef DIVINE_META_H
#define DIVINE_META_H

namespace divine {
namespace meta {

struct Input : WithReport {
    std::string modelType;
    std::string model;
    std::string trace;
    graph::PropertySet properties;
    graph::PropertyType propertyType;
    std::string propertyDetails;
    bool dummygen;
    bool probabilistic;

    std::vector< std::string > definitions;

    Input() : propertyType( graph::PT_None ), dummygen( false ), probabilistic( false ) {}

    std::vector< ReportLine > report() const override;
};

struct Execution : WithReport {
    intptr_t initialTable;
    bool diskFifo;

    int threads;
    int nodes;
    int thisNode;

    Execution() : initialTable( 32 ), diskFifo( false ), threads( 2 ), nodes( 1 ), thisNode( 0 ) {}

    std::vector< ReportLine > report() const override;
};

struct Algorithm : WithReport {
    enum class Type { Metrics, Reachability, WeakReachability, Csdr,
                Ndfs, Map, Owcty, Verify,
                Draw, Info, GenExplicit, Probabilistic, Simulate };
    enum class Compression { None, Tree };

    Type algorithm;
    std::string name;
    uint32_t hashSeed;

    int maxDistance;

    bool labels, traceLabels, bfsLayout; /* for drawing */
    bool hashCompaction;
    Compression compression;
    bool sharedVisitor;
    bool fairness;
    graph::ReductionSet reduce;
    graph::DemangleStyle demangle; // for llvm
    bool interactive; // for simulate
    std::string instance;
    int contextSwitchLimit;

    Algorithm() : algorithm( Type::Info ),
                  hashSeed( 0 ),
                  maxDistance( 32 ),
                  labels( false ), traceLabels( false ), bfsLayout( false ),
                  hashCompaction( false ),
                  compression( Compression::None ),
                  sharedVisitor( false ),
                  fairness( false ),
                  interactive( false ),
                  contextSwitchLimit( -1 )
    {}

    std::vector< ReportLine > report() const override;
};

struct Output : WithReport {
    bool quiet;
    bool wantCe, displayCe;
    bool statistics;
    std::string filterProgram;
    std::string file;
    bool saveStates; // for compact

    Output() : quiet( false ), wantCe( false ), statistics( false ),
        saveStates( false )
    { }

    std::vector< ReportLine > report() const override;
};

struct Statistics : WithReport {
    int64_t visited, expanded, deadlocks, transitions, accepting;
    Statistics() : visited( 0 ), expanded( 0 ), deadlocks( 0 ), transitions( 0 ), accepting( 0 ) {}

    std::vector< ReportLine > report() const override;
};

struct Result : WithReport {
    enum class R { Yes, No, Unknown };
    enum class CET { NoCE, Cycle, Deadlock, Goal };
    R propertyHolds, fullyExplored;
    CET ceType;
    std::vector< int > iniTrail, cycleTrail;
    Result() :
        propertyHolds( R::Unknown ), fullyExplored( R::Unknown ),
        ceType( CET::NoCE ), iniTrail{ -1 }, cycleTrail{ -1 }
    { }

    std::vector< ReportLine > report() const override;
};

}

struct Meta : WithReport {
    meta::Input input;
    meta::Algorithm algorithm;
    meta::Execution execution;
    meta::Output output;
    meta::Result result;
    meta::Statistics statistics;

    std::vector< ReportLine > report() const override;
};

}

namespace std {

inline std::string to_string( divine::meta::Result::R v ) {
    return v == divine::meta::Result::R::Unknown
        ? "Unknown"
        : (v == divine::meta::Result::R::Yes ? "Yes" : "No");
}

inline std::string to_string( divine::meta::Result::CET t ) {
    switch (t) {
        case divine::meta::Result::CET::NoCE: return "none";
        case divine::meta::Result::CET::Goal: return "goal";
        case divine::meta::Result::CET::Cycle: return "cycle";
        case divine::meta::Result::CET::Deadlock: return "deadlock";
        default: ASSERT_UNREACHABLE( "unknown CE type" );
    }
}

inline std::string to_string( divine::meta::Algorithm::Compression c ) {
    switch ( c ) {
        case divine::meta::Algorithm::Compression::None: return "None";
        case divine::meta::Algorithm::Compression::Tree: return "Tree";
        default: ASSERT_UNREACHABLE( "unknown compression" );
    }
}

}

#endif
