#pragma once

#include <algorithm>
#include <type_traits>
#include <vector>
#include <set>
#include <list>
#include <string>

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

namespace tests {

template< typename I, typename T >
void check( I first, I last, std::initializer_list< T > expected ) {
    bool result = std::equal( first, last, expected.begin(), expected.end() );
    REQUIRE( result );
}

//  copy
//  prefix & postfix increment
//  (in)equality
template< typename I >
void iteratorManipulation( I original ) {

    I other = original;
    I next = original;

    REQUIRE( original == next );
    ++next;
    REQUIRE( next != original );
    REQUIRE( original == other++ );
    REQUIRE( original != other );
    REQUIRE( other == next );
}

template< typename I >
void iteratorContent( I original, const std::string &desired ) {
    REQUIRE( *original == desired );
    REQUIRE( original->size() == desired.size() );
}

template< typename T >
using Vector = std::vector< T >;

template< typename T >
using Set = std::set< T >;

template< typename T >
using List = std::list< T >;



} // namespace tests

