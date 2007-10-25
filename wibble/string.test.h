/* -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>
               (c) 2007 Enrico Zini <enrico@enricozini.org> */

#include <wibble/test.h>
#include <wibble/string.h>

namespace {

using namespace std;
using namespace wibble;

struct TestString {

    Test fmt()
    {
        assert_eq(str::fmt(5), "5");
        assert_eq(str::fmt(5.123), "5.123");
        assert_eq(str::fmt("ciao"), "ciao");
    }

    Test basename()
    {
        assert_eq(str::basename("ciao"), "ciao");
        assert_eq(str::basename("a/ciao"), "ciao");
        assert_eq(str::basename("a/b/c/c/d/e/ciao"), "ciao");
        assert_eq(str::basename("/ciao"), "ciao");
    }

    Test dirname()
    {
        assert_eq(str::dirname("ciao"), "");
        assert_eq(str::dirname("a/ciao"), "a");
        assert_eq(str::dirname("a/b/c/c/d/e/ciao"), "a/b/c/c/d/e");
        assert_eq(str::dirname("/a/ciao"), "/a");
        assert_eq(str::dirname("/ciao"), "/");
    }

    Test trim()
    {
        assert_eq(str::trim("   "), "");
        assert_eq(str::trim(" c  "), "c");
        assert_eq(str::trim("ciao"), "ciao");
        assert_eq(str::trim(" ciao"), "ciao");
        assert_eq(str::trim("    ciao"), "ciao");
        assert_eq(str::trim("ciao "), "ciao");
        assert_eq(str::trim("ciao    "), "ciao");
        assert_eq(str::trim(" ciao "), "ciao");
        assert_eq(str::trim("      ciao    "), "ciao");
    }

    Test trim2()
    {
        assert_eq(str::trim(string("ciao"), ::isalpha), "");
        assert_eq(str::trim(" ", ::isalpha), " ");
    }

    Test tolower()
    {
        assert_eq(str::tolower("ciao"), "ciao");
        assert_eq(str::tolower("CIAO"), "ciao");
        assert_eq(str::tolower("Ciao"), "ciao");
        assert_eq(str::tolower("cIAO"), "ciao");
    }

    Test toupper()
    {
        assert_eq(str::toupper("ciao"), "CIAO");
        assert_eq(str::toupper("CIAO"), "CIAO");
        assert_eq(str::toupper("Ciao"), "CIAO");
        assert_eq(str::toupper("cIAO"), "CIAO");
    }

// Check startsWith
    Test startsWith()
    {
        assert(str::startsWith("ciao", "ci"));
        assert(str::startsWith("ciao", ""));
        assert(str::startsWith("ciao", "ciao"));
        assert(!str::startsWith("ciao", "ciaoa"));
        assert(!str::startsWith("ciao", "i"));
    }

    Test endsWith()
    {
        assert(str::endsWith("ciao", "ao"));
        assert(str::endsWith("ciao", ""));
        assert(str::endsWith("ciao", "ciao"));
        assert(!str::endsWith("ciao", "aciao"));
        assert(!str::endsWith("ciao", "a"));
    }

    Test joinpath()
    {
        assert_eq(str::joinpath("a", "b"), "a/b");
        assert_eq(str::joinpath("a/", "b"), "a/b");
        assert_eq(str::joinpath("a", "/b"), "a/b");
        assert_eq(str::joinpath("a/", "/b"), "a/b");
    }

};

}

// vim:set ts=4 sw=4:
