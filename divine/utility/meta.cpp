#include <divine/utility/meta.h>

using namespace divine;

namespace divine {
using Rep = std::vector< ReportPair >;

namespace meta {

std::string tostr( Result::R v ) {
    return v == Result::R::Unknown
        ? "Unknown"
        : (v == Result::R::Yes ? "Yes" : "No");
}

std::string tostr( Result::CET t ) {
    switch (t) {
        case Result::CET::NoCE: return "none";
        case Result::CET::Goal: return "goal";
        case Result::CET::Cycle: return "cycle";
        case Result::CET::Deadlock: return "deadlock";
    }
}

std::string tostr( graph::PropertyType t )
{
    switch (t) {
        case graph::PT_Deadlock: return "deadlock";
        case graph::PT_Goal: return "goal";
        case graph::PT_Buchi: return "neverclaim";
        default: return "unknonw";
    }
}

template< typename T >
std::string tostr( T t ) {
    return std::to_string( t );
}

Rep Input::report() const {
    return { { "Property-Type", tostr( propertyType ) },
             { "Property-Name", propertyName.empty() ? "-" : propertyName },
             { "Property", property.empty() ? "-" : property }
           };
}

Rep Result::report() const {
    return { { "Property-Holds", tostr( propertyHolds ) },
             { "Full-State-Space", tostr( fullyExplored ) },
             { "CE-Type", tostr( ceType ) },
             { "CE-Init", iniTrail },
             { "CE-Cycle", cycleTrail }
           };
}

Rep Output::report() const {
    return { };
}

Rep Statistics::report() const {
    return { { "States-Visited", tostr( visited ) },
             { "States-Accepting", tostr( accepting ) },
             { "States-Expanded", tostr( expanded ) },
             { "Transition-Count", transitions >= 0 ? tostr( transitions ) : "-" },
             { "Deadlock-Count", deadlocks >= 0 ? tostr( deadlocks ) : "-" }
           };
}

Rep Algorithm::report() const {
    std::vector< std::string > txt;

    for ( auto r = reduce.begin(); r != reduce.end(); ++r )
        switch ( *r ) {
            case graph::R_POR: txt.push_back( "POR" ); break;
            case graph::R_TauPlus: txt.push_back( "tau+" ); break;
            case graph::R_TauStores: txt.push_back( "taustores" ); break;
            case graph::R_Tau: txt.push_back( "tau" ); break;
            case graph::R_Heap: txt.push_back( "heap" ); break;
            case graph::R_LU: txt.push_back( "LU" ); break;
        }

    if ( fairness )
        txt.push_back( "fairness" );

    return { { "Algorithm", name },
             { "Transformations", wibble::str::fmt( txt ) }
           };
}

Rep Execution::report() const {
    return { { "Threads", tostr( threads ) },
             { "MPI-Nodes", tostr( nodes ) }
           };
}
}

Rep Meta::report() const {
    ReportPair empty = { "", "" };
    return WithReport::merge( input, empty, algorithm, empty, execution,
            empty, result, empty, statistics );
}

}
