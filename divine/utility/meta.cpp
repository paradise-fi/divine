#include <divine/utility/meta.h>

using namespace divine;
using namespace meta;

namespace divine {

std::ostream &operator<<( std::ostream &o, Result::R v )
{
    return o << (v == Result::Unknown ? "-" :
                 (v == Result::Yes ? "Yes" : "No" ) );
}

std::ostream &operator<<( std::ostream &o, Result::CET t )
{
    switch (t) {
        case Result::NoCE: return o << "none";
        case Result::Goal: return o << "goal";
        case Result::Cycle: return o << "cycle";
        case Result::Deadlock: return o << "deadlock";
    }
    return o;
}

std::ostream &operator<<( std::ostream &o, graph::PropertyType t )
{
    switch (t) {
        case graph::PT_Deadlock: return o << "deadlock";
        case graph::PT_Goal: return o << "goal";
        case graph::PT_Buchi: return o << "neverclaim";
    }
    return o;
}

std::ostream &operator<<( std::ostream &o, Input i )
{
    o << "Property-Type: " << i.propertyType << std::endl;
    o << "Property-Name: " << (i.propertyName.empty() ? "-" : i.propertyName) << std::endl;
    o << "Property: " << (i.property.empty() ? "-" : i.property) << std::endl;
    return o;
}

std::ostream &operator<<( std::ostream &o, Result r )
{
    o << "Property-Holds: " << r.propertyHolds << std::endl;
    o << "Full-State-Space: " << r.fullyExplored << std::endl;
    o << "CE-Type: " << r.ceType << std::endl;
    o << "CE-Init: " << r.iniTrail << std::endl;
    o << "CE-Cycle: " << r.cycleTrail << std::endl;
    return o;
}

std::ostream &operator<<( std::ostream &o, Statistics r )
{
    o << "States-Visited: " << r.visited << std::endl;
    o << "States-Accepting: " << r.accepting << std::endl;
    o << "States-Expanded: " << r.expanded << std::endl;

    o << "Transition-Count: ";
    if (r.transitions >= 0)
        o << r.transitions << std::endl;
    else
        o << "-" << std::endl;

    o << "Deadlock-Count: ";
    if (r.deadlocks >= 0)
        o << r.deadlocks << std::endl;
    else
        o << "-" << std::endl;

    return o;
}

std::ostream &operator<<( std::ostream &o, Algorithm a )
{
    o << "Algorithm: " << a.name << std::endl;
    o << "Transformations: ";
    std::vector< std::string > txt;

    for ( auto r = a.reduce.begin(); r != a.reduce.end(); ++r )
        switch ( *r ) {
            case graph::R_POR: txt.push_back( "POR" ); break;
            case graph::R_TauPlus: txt.push_back( "tau+" ); break;
            case graph::R_TauStores: txt.push_back( "taustores" ); break;
            case graph::R_Tau: txt.push_back( "tau" ); break;
            case graph::R_Heap: txt.push_back( "heap" ); break;
        }

    if ( a.fairness )
        txt.push_back( "fairness" );

    return o << wibble::str::fmt( txt ) << std::endl;
}

std::ostream &operator<<( std::ostream &o, Execution a )
{
    o << "Threads: " << a.threads << std::endl;
    o << "MPI-Nodes: "<< a.nodes << std::endl;
    return o;
}

std::ostream &operator<<( std::ostream &o, Meta m )
{
    o << m.input << std::endl;
    o << m.algorithm << std::endl;
    o << m.execution << std::endl;
    o << m.result << std::endl;
    o << m.statistics;
    return o;
}

}
