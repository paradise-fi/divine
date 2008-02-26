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

    Test urlencode()
    {
        assert_eq(str::urlencode(""), "");
        assert_eq(str::urlencode("antani"), "antani");
        assert_eq(str::urlencode("a b c"), "a%20b%20c");
        assert_eq(str::urlencode("a "), "a%20");

        assert_eq(str::urldecode(""), "");
        assert_eq(str::urldecode("antani"), "antani");
        assert_eq(str::urldecode("a%20b"), "a b");
        assert_eq(str::urldecode("a%20"), "a ");
        assert_eq(str::urldecode("a%2"), "a");
        assert_eq(str::urldecode("a%"), "a");

        assert_eq(str::urldecode(str::urlencode("àá☣☢☠!@#$%^&*(\")/A")), "àá☣☢☠!@#$%^&*(\")/A");
        assert_eq(str::urldecode(str::urlencode("http://zz:ss@a.b:31/c?d=e&f=g")), "http://zz:ss@a.b:31/c?d=e&f=g");
    }

	Test split1()
	{
		string val = "";
		str::Split split("/", val);
		str::Split::const_iterator i = split.begin();
		assert(i == split.end());
	}

	Test split2()
	{
		string val = "foo";
		str::Split split("/", val);
		str::Split::const_iterator i = split.begin();
		assert(i != split.end());
		assert_eq(*i, "foo");
		assert_eq(i.remainder(), "");
		++i;
		assert(i == split.end());
	}

	Test split3()
	{
		string val = "foo";
		str::Split split("", val);
		str::Split::const_iterator i = split.begin();
		assert(i != split.end());
		assert_eq(*i, "f");
		assert_eq(i.remainder(), "oo");
		++i;
		assert_eq(*i, "o");
		assert_eq(i.remainder(), "o");
		++i;
		assert_eq(*i, "o");
		assert_eq(i.remainder(), "");
		++i;
		assert(i == split.end());
	}

	Test split4()
	{
		string val = "/a//foo/";
		str::Split split("/", val);
		str::Split::const_iterator i = split.begin();
		assert(i != split.end());
		assert_eq(*i, "");
		assert_eq(i.remainder(), "a//foo/");
		++i;
		assert(i != split.end());
		assert_eq(*i, "a");
		assert_eq(i.remainder(), "/foo/");
		++i;
		assert(i != split.end());
		assert_eq(*i, "");
		assert_eq(i.remainder(), "foo/");
		++i;
		assert(i != split.end());
		assert_eq(*i, "foo");
		assert_eq(i.remainder(), "");
		++i;
		assert(i == split.end());
	}

	Test normpath()
	{
		assert_eq(str::normpath(""), ".");
		assert_eq(str::normpath("/"), "/");
		assert_eq(str::normpath("foo"), "foo");
		assert_eq(str::normpath("foo/"), "foo");
		assert_eq(str::normpath("/foo"), "/foo");
		assert_eq(str::normpath("foo/bar"), "foo/bar");
		assert_eq(str::normpath("foo/./bar"), "foo/bar");
		assert_eq(str::normpath("././././foo/./././bar/././././"), "foo/bar");
		assert_eq(str::normpath("/../../../../../foo"), "/foo");
		assert_eq(str::normpath("foo/../foo/../foo/../foo/../"), ".");
		assert_eq(str::normpath("foo//bar"), "foo/bar");
		assert_eq(str::normpath("foo/./bar"), "foo/bar");
		assert_eq(str::normpath("foo/foo/../bar"), "foo/bar");
	}
};

}

// vim:set ts=4 sw=4:
