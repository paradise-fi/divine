// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * Various assert macros based on C++ exceptions and their support code.
 */

/*
 * (c) 2006-2014 Petr Ročkai <me@mornfall.net>
 */

/* Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. */

#include <exception>
#include <string>
#include <sstream>

#ifndef BRICK_ASSERT_H
#define BRICK_ASSERT_H

#ifdef __divine__
#include <divine.h>
#endif

#ifndef TEST
#define TEST(n)         void n()
#define TEST_FAILING(n) void n()
#endif

#ifdef __divine__
#define ASSERT(x) assert( x )
#define ASSERT_PRED(p, x) assert( p( x ) )
#define ASSERT_EQ(x, y) assert( (x) == (y) )
#define ASSERT_LEQ(x, y) assert( (x) <= (y) )
#define ASSERT_NEQ(x, y) assert ( (x) != (y) )

#elif !defined NDEBUG
#define BRICK_LOCATION_I(stmt, i) ::brick::assert::Location( __FILE__, __LINE__, stmt, i )

#define ASSERT(x) assert_fn( BRICK_LOCATION( #x ), x )
#define ASSERT_PRED(p, x) assert_pred_fn( BRICK_LOCATION( #p "( " #x " )" ), x, p( x ) )
#define ASSERT_EQ(x, y) assert_eq_fn( BRICK_LOCATION( #x " == " #y ), x, y )
#define ASSERT_LEQ(x, y) assert_leq_fn( BRICK_LOCATION( #x " <= " #y ), x, y )
#define ASSERT_NEQ(x, y) assert_neq_fn( BRICK_LOCATION( #x " != " #y ), x, y )
#define ASSERT_EQ_IDX(i, x, y) assert_eq_fn( BRICK_LOCATION( brick::string::fmtf( "%s at index %d", #x " == " #y, i ) ), x, y )

#else

#define ASSERT(x) ((void)0)
#define ASSERT_PRED(p, x) ((void)0)
#define ASSERT_EQ(x, y) ((void)0)
#define ASSERT_LEQ(x, y) ((void)0)
#define ASSERT_NEQ(x, y) ((void)0)
#endif

/* you must #include <brick-string.h> to use ASSERT_UNREACHABLE_F */
#define ASSERT_UNREACHABLE_F(...) assert_die_fn( BRICK_LOCATION( brick::string::fmtf(__VA_ARGS__) ) )
#define ASSERT_UNREACHABLE(x) assert_die_fn( BRICK_LOCATION( x ) )
#define ASSERT_UNIMPLEMENTED() assert_die_fn( BRICK_LOCATION( "not imlemented" ) )

#define UNUSED __attribute__((unused))

namespace brick {
namespace _assert {

/* discard any number of paramentets, taken as const references */
template< typename... X >
void unused( const X&... ) { }

struct Location {
    const char *file;
    int line, iteration;
    std::string stmt;
    Location( const char *f, int l, std::string st, int iter = -1 )
        : file( f ), line( l ), iteration( iter ), stmt( st ) {}
};

#define BRICK_LOCATION(stmt) ::brick::_assert::Location( __FILE__, __LINE__, stmt )

struct AssertFailed : std::exception {
    std::string str;

    template< typename X >
    friend inline AssertFailed &operator<<( AssertFailed &f, X x )
    {
        std::stringstream str;
        str << x;
        f.str += str.str();
        return f;
    }

    AssertFailed( Location l )
    {
        (*this) << l.file << ": " << l.line;
        if ( l.iteration != -1 )
            (*this) << " (iteration " << l.iteration << ")";
        (*this) << ": assertion `" << l.stmt << "' failed;";
    }

    const char *what() const noexcept { return str.c_str(); }
};

template< typename X >
void assert_fn( Location l, X x )
{
    if ( !x ) {
        throw AssertFailed( l );
    }
}

inline void assert_die_fn( Location l ) __attribute__((noreturn));

inline void assert_die_fn( Location l )
{
    throw AssertFailed( l );
}

template< typename X, typename Y >
void assert_eq_fn( Location l, X x, Y y )
{
    if ( !( x == y ) ) {
        AssertFailed f( l );
        f << " got ["
          << x << "] != [" << y
          << "] instead";
        throw f;
    }
}

template< typename X, typename Y >
void assert_leq_fn( Location l, X x, Y y )
{
    if ( !( x <= y ) ) {
        AssertFailed f( l );
        f << " got ["
          << x << "] > [" << y
          << "] instead";
        throw f;
    }
}

template< typename X >
void assert_pred_fn( Location l, X x, bool p )
{
    if ( !p ) {
        AssertFailed f( l );
        f << " for " << x;
        throw f;
    }
}

template< typename X, typename Y >
void assert_neq_fn( Location l, X x, Y y )
{
    if ( x != y )
        return;
    AssertFailed f( l );
    f << " got ["
      << x << "] == [" << y << "] instead";
    throw f;
}

}
}

#endif

// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab
