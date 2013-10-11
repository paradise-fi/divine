// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <divine/algorithm/common.h>

#include <iostream>
#include <string>
#include <sstream>
#include <type_traits>

#ifndef DIVINE_INSTANCES_CREATE_H
#define DIVINE_INSTANCES_CREATE_H

namespace divine {
namespace instantiate {

using AlgoR = std::unique_ptr< ::divine::algorithm::Algorithm >;

template< typename I >
auto _show( std::stringstream &, I )
        -> typename std::enable_if< I::length == 0 >::type
{ }

template< typename I >
auto _show( std::stringstream &ss, I )
        -> typename std::enable_if< ( I::length > 0 ) >::type
{
    ss << I::Head::key << ", ";
    _show( ss, typename I::Tail() );
}

template< typename Instance >
std::string showInstance( Instance ) {
    std::stringstream ss;
    ss << "[ ";
    _show( ss, Instance() );
    ss.seekp( -2, std::ios_base::cur );
    ss << " ]";
    return ss.str();
}

template< typename Selected >
struct Setup { };

template< typename Selected >
AlgoR createInstance( Meta & ) {
    std::cerr << "ERROR: missing instance: "
        << showInstance( Selected() ) << std::endl;
    return nullptr;
}

}
}

#endif // DIVINE_INSTANCES_CREATE_H
