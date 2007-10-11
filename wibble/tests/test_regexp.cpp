#include <wibble/config.h>
#include <wibble/regexp.h>
using namespace std;
using namespace wibble;

#include <wibble/tests/tut-wibble.h>

namespace tut {

struct wibble_regexp_shar {};
TESTGRP( wibble_regexp );

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
	ensure_equals(re.matchStart(0), 0u);
	ensure_equals(re.matchEnd(0), 5u);
	ensure_equals(re.matchLength(0), 5u);
	ensure_equals(re.matchStart(1), 1u);
	ensure_equals(re.matchEnd(1), 2u);
	ensure_equals(re.matchLength(1), 1u);

	ensure(re.match("foobar42"));
	ensure_equals(re[0], string("foobar42"));
	ensure_equals(re[1], string("oo"));
	ensure_equals(re[2], string("42"));
}

// Test tokenizer
template<> template<>
void to::test< 4 >() {
	string str("antani blinda la supercazzola!");
	Tokenizer tok(str, "[a-z]+", REG_EXTENDED);
	Tokenizer::const_iterator i = tok.begin();

	ensure(i != tok.end());
	ensure_equals(*i, "antani");
	++i;
	ensure(i != tok.end());
	ensure_equals(*i, "blinda");
	++i;
	ensure(i != tok.end());
	ensure_equals(*i, "la");
	++i;
	ensure(i != tok.end());
	ensure_equals(*i, "supercazzola");
	++i;
	ensure(i == tok.end());
}

}

// vim:set ts=4 sw=4:
