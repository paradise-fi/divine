#include <divine/meta.h>

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

std::ostream &operator<<( std::ostream &o, Input::PT t )
{
    switch (t) {
        case Input::NoProperty: return o << "none";
        case Input::Neverclaim: return o << "neverclaim";
        case Input::LTL: return o << "LTL";
        case Input::Reachability: return o << "reachabality";
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
    o << "States-Expanded: " << r.expanded << std::endl;
    o << "Transition-Count: " << r.transitions << std::endl;
    o << "Deadlock-Count: " << r.deadlocks << std::endl;
    return o;
}

std::ostream &operator<<( std::ostream &o, Algorithm a )
{
    o << "Algorithm: " << a.name << std::endl;
    o << "Transformations: ";
    if ( a.transformations.empty() )
        o << "None";
    else {
        o << a.transformations.front();
        for ( std::vector< std::string >::iterator i = a.transformations.begin() + 1;
              i != a.transformations.end(); ++i )
            o << ", " << *i;
    }
    o << std::endl;

    return o;
}

std::ostream &operator<<( std::ostream &o, Execution a )
{
    o << "Workers: " << a.workers << std::endl;
    o << "MPI: ";
    if (a.mpi > 1)
        o << a.mpi << " nodes";
    else
        o << "not used";
    o << std::endl;
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
