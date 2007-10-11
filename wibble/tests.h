#ifndef WIBBLE_TESTS_H
#define WIBBLE_TESTS_H

/**
 * @file test-utils.h
 * @author Peter Rockai (mornfall) <mornfall@danill.sk>, Enrico Zini <enrico@enricozini.org>
 * @brief Utility functions for the unit tests
 */
#include <tut.h>

#define TESTGRP(name) \
typedef test_group<name ## _shar> tg; \
typedef tg::object to; \
tg name ## _tg (#name);


namespace wibble {
namespace tests {

class Location
{
	std::string file;
	int line;
	std::string str;
	std::string testfile;
	int testline;
	std::string teststr;

public:

	Location(const std::string& file, int line, const std::string& str)
		: file(file), line(line), str(str) {}
	Location(const Location& loc,
		 const std::string& testfile, int testline, const std::string& str) :
		file(loc.file), line(loc.line), str(loc.str),
		testfile(testfile), testline(testline), teststr(str) {}

	std::string locstr() const;
	std::string msg(const std::string m) const;
};

#define ensure(x) wibble::tests::impl_ensure(Location(__FILE__, __LINE__, #x), (x))
#define inner_ensure(x) wibble::tests::impl_ensure(Location(loc, __FILE__, __LINE__, #x), (x))
void impl_ensure(const Location& loc, bool res);

#define ensure_equals(x, y) wibble::tests::impl_ensure_equals(Location(__FILE__, __LINE__, #x " == " #y), (x), (y))
#define inner_ensure_equals(x, y) wibble::tests::impl_ensure_equals(Location(loc, __FILE__, __LINE__, #x " == " #y), (x), (y))
template <class T,class Q>
void impl_ensure_equals(const Location& loc, const Q& actual, const T& expected)
{
	if( expected != actual )
	{
		std::stringstream ss;
		ss << "expected " << expected << " actual " << actual;
		throw tut::failure(loc.msg(ss.str()));
	}
}

}
}

#endif
