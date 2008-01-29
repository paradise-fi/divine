// -*- C++ -*-

#include <iostream>
#include <cstdlib>

#ifndef WIBBLE_TEST_H
#define WIBBLE_TEST_H

// TODO use TLS
extern int assertFailure;

struct Location {
    const char *file;
    int line;
    std::string stmt;
    Location( const char *f, int l, std::string st )
        : file( f ), line( l ), stmt( st ) {}
};

#define LOCATION(stmt) Location( __FILE__, __LINE__, stmt )
#define assert(x) assert_fn( LOCATION( #x ), x )
#define assert_eq(x, y) assert_eq_fn( LOCATION( #x " == " #y ), x, y )
#define assert_neq(x, y) assert_neq_fn( LOCATION( #x " != " #y ), x, y )

#define CHECK_ASSERT(x)                                                 \
    do {                                                                \
        if ( x ) return;                                                \
        else if ( assertFailure )                                       \
        {                                                               \
            ++assertFailure;                                            \
            return;                                                     \
        }                                                               \
    } while (0)

static inline std::ostream &assert_failed( Location l )
{
    return std::cerr << l.file << ": " << l.line
                     << ": assertion `" << l.stmt << "' failed;";
}

template< typename X >
void assert_fn( Location l, X x )
{
    CHECK_ASSERT( x );
    assert_failed( l ) << std::endl;
    abort();
}

template< typename X, typename Y >
void assert_eq_fn( Location l, X x, Y y )

{
    CHECK_ASSERT( x == y );
    assert_failed( l ) << " got ["
                       << x << "] != [" << y << "] instead" << std::endl;
    abort();
}

template< typename X, typename Y >
void assert_neq_fn( Location l, X x, Y y )

{
    CHECK_ASSERT( x != y );
    assert_failed( l ) << " got ["
                       << x << "] == [" << y << "] instead" << std::endl;
    abort();
}

inline void beginAssertFailure() {
    assertFailure = 1;
}

inline void endAssertFailure() {
    int f = assertFailure;
    assertFailure = 0;
    assert( f > 1 );
}

struct AssertFailure {
    AssertFailure() { beginAssertFailure(); }
    ~AssertFailure() { endAssertFailure(); }
};

typedef void Test;

#endif
