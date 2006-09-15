#include <wibble/config.h>
#include <wibble/regexp.h>
using namespace std;
using namespace wibble;

#include <wibble/tests/tut-wibble.h>

namespace tut {

struct regexp_shar {};
TESTGRP( regexp );

// Test normal regular expression matching
template<> template<>
void to::test< 1 >() {
	Regexp re("^fo\\+bar()$");
	ensure(re.match("fobar()"));
	ensure(re.match("foobar()"));
	ensure(re.match("fooobar()"));
	ensure(!re.match("fbar()"));
	ensure(!re.match(" foobar()"));
	ensure(!re.match("foobar() "));
}

// Test extended regular expression matching
template<> template<>
void to::test< 2 >() {
	ERegexp re("^fo+bar()$");
	ensure(re.match("fobar"));
	ensure(re.match("foobar"));
	ensure(re.match("fooobar"));
	ensure(!re.match("fbar"));
	ensure(!re.match(" foobar"));
	ensure(!re.match("foobar "));
}

// Test capturing results
template<> template<>
void to::test< 3 >() {
	ERegexp re("^f(o+)bar([0-9]*)$", 3);
	ensure(re.match("fobar"));
	ensure_equals(re[0], string("fobar"));
	ensure_equals(re[1], string("o"));
	ensure_equals(re[2], string(""));

	ensure(re.match("foobar42"));
	ensure_equals(re[0], string("foobar42"));
	ensure_equals(re[1], string("oo"));
	ensure_equals(re[2], string("42"));
}

}

// vim:set ts=4 sw=4:
