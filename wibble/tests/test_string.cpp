#include <wibble/config.h>
#include <wibble/string.h>
using namespace std;
using namespace wibble;

#include <wibble/tests/tut-wibble.h>

namespace tut {

struct wibble_string_shar {};
TESTGRP( wibble_string );

// Check fmt
template<> template<>
void to::test<1>()
{
	ensure_equals(str::fmt(5), "5");
	ensure_equals(str::fmt(5.123), "5.123");
	ensure_equals(str::fmt("ciao"), "ciao");
}

// Check basename
template<> template<>
void to::test<2>()
{
	ensure_equals(str::basename("ciao"), "ciao");
	ensure_equals(str::basename("a/ciao"), "ciao");
	ensure_equals(str::basename("a/b/c/c/d/e/ciao"), "ciao");
	ensure_equals(str::basename("/ciao"), "ciao");
}

// Check trim
template<> template<>
void to::test<3>()
{
	ensure_equals(str::trim("   "), "");
	ensure_equals(str::trim(" c  "), "c");
	ensure_equals(str::trim("ciao"), "ciao");
	ensure_equals(str::trim(" ciao"), "ciao");
	ensure_equals(str::trim("    ciao"), "ciao");
	ensure_equals(str::trim("ciao "), "ciao");
	ensure_equals(str::trim("ciao    "), "ciao");
	ensure_equals(str::trim(" ciao "), "ciao");
	ensure_equals(str::trim("      ciao    "), "ciao");
}

// Check trim
template<> template<>
void to::test<4>()
{
	ensure_equals(str::trim(string("ciao"), ::isalpha), "");
	ensure_equals(str::trim(" ", ::isalpha), " ");
}

// Check tolower
template<> template<>
void to::test<5>()
{
	ensure_equals(str::tolower("ciao"), "ciao");
	ensure_equals(str::tolower("CIAO"), "ciao");
	ensure_equals(str::tolower("Ciao"), "ciao");
	ensure_equals(str::tolower("cIAO"), "ciao");
}

// Check toupper
template<> template<>
void to::test<6>()
{
	ensure_equals(str::toupper("ciao"), "CIAO");
	ensure_equals(str::toupper("CIAO"), "CIAO");
	ensure_equals(str::toupper("Ciao"), "CIAO");
	ensure_equals(str::toupper("cIAO"), "CIAO");
}

// Check startsWith
template<> template<>
void to::test<7>()
{
	ensure(str::startsWith("ciao", "ci"));
	ensure(str::startsWith("ciao", ""));
	ensure(str::startsWith("ciao", "ciao"));
	ensure(!str::startsWith("ciao", "ciaoa"));
	ensure(!str::startsWith("ciao", "i"));
}

// Check endsWith
template<> template<>
void to::test<8>()
{
	ensure(str::endsWith("ciao", "ao"));
	ensure(str::endsWith("ciao", ""));
	ensure(str::endsWith("ciao", "ciao"));
	ensure(!str::endsWith("ciao", "aciao"));
	ensure(!str::endsWith("ciao", "a"));
}

}

// vim:set ts=4 sw=4:
