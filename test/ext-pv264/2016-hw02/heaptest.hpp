#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <type_traits>
#include <random>

// Test that all Heap functions instantiate at least for int.
template struct Heap< int, std::less< int > >;

TEST_CASE( "Empty heap" ) {
    MaxHeap< int > h;
    REQUIRE( h.empty() );
    REQUIRE( h.size() == 0 );
}

TEST_CASE( "Singleton heap" ) {
    MaxHeap< int > heap;
    auto h = heap.insert( 1 );
    REQUIRE_FALSE( heap.empty() );
    REQUIRE( heap.size() == 1 );
    REQUIRE( heap.top() == 1 );
    REQUIRE( heap.get( h ) == 1 );
}

TEST_CASE( "Two elements" ) {
    MaxHeap< int > heap;
    auto h = heap.insert( 1 );
    MaxHeap< int >::Handle h2;
    int inserted;

    SECTION( "second bigger" ) {
        h2 = heap.insert( inserted = 2 );
        REQUIRE( heap.top() == 2 );
        REQUIRE( heap.topHandle() == h2 );
    }
    SECTION( "second smaller" ) {
        h2 = heap.insert( inserted = 0 );
        REQUIRE( heap.top() == 1 );
        REQUIRE( heap.topHandle() == h );
    }

    REQUIRE( heap.get( h ) == 1 );
    REQUIRE( heap.get( h2 ) == inserted );
    REQUIRE( heap.size() == 2 );
}

TEST_CASE( "List constructor small" ) {
    MaxHeap< int > heap = { 1, 42, 4 };
    auto sorted = toSortedVector( heap );
    REQUIRE( heap.size() == 3 );
    REQUIRE( sorted[ 0 ] == 42 );
    REQUIRE( sorted[ 1 ] == 4 );
    REQUIRE( sorted[ 2 ] == 1 );
}

TEST_CASE( "List constructor big" ) {
    auto list = { 1, 42, 15, 4, 3, 8, 17, 9, 0, -42, 15 };
    std::vector< int > sorted{ list };
    std::sort( sorted.begin(), sorted.end(), []( int x, int y ) { return x > y; } );
    MaxHeap< int > heap( list );
    REQUIRE( heap.size() == list.size() );
    REQUIRE( sorted == toSortedVector( heap ) );
}

bool gt( int x, int y ) { return x > y; }

TEST_CASE( "Iterator constructor" ) {
    std::vector< int > sorted{ 1, 42, 15, 4, 3, 8, 17, 9, 0, -42, 15 };
    MaxHeap< int > heap( sorted.begin(), sorted.end() );
    std::sort( sorted.begin(), sorted.end(), gt );
    REQUIRE( heap.size() == sorted.size() );
    REQUIRE( sorted == toSortedVector( heap ) );
}

/*
TEST_CASE( "Large random heapify" ) {
    std::vector< int > random( 1024 * 1024 );
    std::mt19937 randgen;
    std::generate( random.begin(), random.end(), randgen );
    MaxHeap< int > heap( random.begin(), random.end() );
    std::sort( random.begin(), random.end(), gt );
    REQUIRE( heap.size() == random.size() );
    REQUIRE( random == toSortedVector( heap ) );
}

TEST_CASE( "Large random insert" ) {
    std::vector< int > random( 1024 * 1024 );
    std::mt19937 randgen;
    std::generate( random.begin(), random.end(), randgen );
    MaxHeap< int > heap;
    for ( int x : random )
        heap.insert( x );
    std::sort( random.begin(), random.end(), gt );
    REQUIRE( heap.size() == random.size() );
    REQUIRE( random == toSortedVector( heap ) );
}
*/

TEST_CASE( "Update" ) {
    MaxHeap< int > heap;
    auto h = heap.insert( 1 );
    MaxHeap< int >::Handle h2;
    int updated;

    SECTION( "second bigger" ) {
        h2 = heap.insert( 2 );
        SECTION( "make smaller" ) {
            heap.update( h2, updated = -2 );
            REQUIRE( heap.top() == 1 );
            REQUIRE( heap.topHandle() == h );
        }
        SECTION( "make bigger" ) {
            heap.update( h2, updated = 4 );
            REQUIRE( heap.top() == 4 );
            REQUIRE( heap.topHandle() == h2 );
        }
    }
    SECTION( "second smaller" ) {
        h2 = heap.insert( 0 );
        SECTION( "make smaller" ) {
            heap.update( h2, updated = -2 );
            REQUIRE( heap.top() == 1 );
            REQUIRE( heap.topHandle() == h );
        }
        SECTION( "make bigger" ) {
            heap.update( h2, updated = 4 );
            REQUIRE( heap.top() == 4 );
            REQUIRE( heap.topHandle() == h2 );
        }
    }

    REQUIRE( heap.get( h ) == 1 );
    REQUIRE( heap.get( h2 ) == updated );
    REQUIRE( heap.size() == 2 );
}

/*
TEST_CASE( "Update random" ) {
    std::vector< int > random( 1024 * 1024 );
    std::mt19937 randgen;
    std::generate( random.begin(), random.end(), randgen );
    MaxHeap< int > heap;
    std::vector< std::pair< MaxHeap< int >::Handle, int > > updates;

    for ( int &x : random ) {
        auto h = heap.insert( x );
        if ( randgen() % 100 == 0 )
            updates.emplace_back( h, x = randgen() );
    }
    std::shuffle( updates.begin(), updates.end(), randgen );
    for ( auto u : updates )
        heap.update( u.first, u.second );

    std::sort( random.begin(), random.end(), gt );
    REQUIRE( heap.size() == random.size() );
    REQUIRE( random == toSortedVector( heap ) );
}
*/

TEST_CASE( "Erase" ) {
    MaxHeap< int > heap;
    auto h = heap.insert( 1 );
    MaxHeap< int >::Handle h2, remh;
    int inserted;

    SECTION( "second bigger" ) {
        h2 = heap.insert( inserted = 2 );
        SECTION( "erase top" ) {
            heap.erase( h2 );
            REQUIRE( heap.top() == 1 );
            remh = h;
        }
        SECTION( "erase other" ) {
            heap.erase( h );
            REQUIRE( heap.top() == 2 );
            remh = h2;
        }
    }
    SECTION( "second smaller" ) {
        h2 = heap.insert( inserted = 0 );
        SECTION( "erase top" ) {
            heap.erase( h );
            REQUIRE( heap.top() == 0 );
            remh = h2;
        }
        SECTION( "erase other" ) {
            heap.erase( h2 );
            REQUIRE( heap.top() == 1 );
            remh = h;
        }
    }
    REQUIRE( heap.size() == 1 );

    heap.erase( remh );
    REQUIRE( heap.size() == 0 );
    REQUIRE( heap.empty() );
}

TEST_CASE( "Erase in a slightly bigger heap" ) {
	MinHeap< int > heap;
	for ( int i : { 1, 5, 2 } ) heap.insert( i );
	auto h = heap.insert( 9 );
	for ( int i : { 8, 3 } )    heap.insert( i );
	heap.erase( h );
	heap.pop();     // 1
	heap.insert(4);
	heap.pop();     // 2
	REQUIRE( heap.top() == 3 );
}

