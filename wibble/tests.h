#ifndef WIBBLE_TESTS_H
#define WIBBLE_TESTS_H

/**
 * @file test-utils.h
 * @author Enrico Zini <enrico@enricozini.org>, Peter Rockai (mornfall) <me@mornfall.net>
 * @brief Utility functions for the unit tests
 *
 * Copyright (C) 2006--2007  Peter Rockai (mornfall) <me@mornfall.net>
 * Copyright (C) 2003--2013  Enrico Zini <enrico@debian.org>
 */

#include <string>
#include <sstream>

#include <wibble/tests/tut.h>
#include <wibble/tests/tut_reporter.h>

namespace wibble {
namespace tests {
struct Location;
struct LocationInfo;
}
}

/*
 * These global arguments will be shadowed by local variables in functions that
 * implement tests.
 *
 * They are here to act as default root nodes to fulfill method signatures when
 * tests are called from outside other tests.
 */
extern const wibble::tests::Location wibble_test_location;
extern const wibble::tests::LocationInfo wibble_test_location_info;

#define TESTGRP(name) \
typedef test_group<name ## _shar> tg; \
typedef tg::object to; \
tg name ## _tg (#name);


namespace wibble {
namespace tests {

#define WIBBLE_TESTS_ALWAYS_THROWS __attribute__ ((noreturn))

class Location
{
    const Location* parent;
    const wibble::tests::LocationInfo* info;
    const char* file;
    int line;
    const char* args;

    Location(const Location* parent, const wibble::tests::LocationInfo& info, const char* file, int line, const char* args);

public:
    Location();
    // legacy
    Location(const char* file, int line, const char* args);
    // legacy
    Location(const Location& parent, const char* file, int line, const char* args);
    Location nest(const wibble::tests::LocationInfo& info, const char* file, int line, const char* args=0) const;

    std::string locstr() const;
    std::string msg(const std::string m) const;
    void fail_test(const std::string& msg) const WIBBLE_TESTS_ALWAYS_THROWS;
    void backtrace(std::ostream& out) const;
};

struct LocationInfo : public std::stringstream
{
    /**
     * Clear the stringstream and return self.
     *
     * Example usage:
     *
     * test_function(...)
     * {
     *    WIBBLE_TEST_INFO(info);
     *    for (unsigned i = 0; i < 10; ++i)
     *    {
     *       info() << "Iteration #" << i;
     *       ...
     *    }
     * }
     */
    std::ostream& operator()();
};

#define WIBBLE_TEST_LOCPRM wibble::tests::Location wibble_test_location

/// Use this to declare a local variable with the given name that will be
/// picked up by tests as extra local info
#define WIBBLE_TEST_INFO(name) \
    wibble::tests::LocationInfo wibble_test_location_info; \
    wibble::tests::LocationInfo& name = wibble_test_location_info

#define ensure(x) wibble::tests::impl_ensure(wibble::tests::Location(__FILE__, __LINE__, #x), (x))
#define inner_ensure(x) wibble::tests::impl_ensure(wibble::tests::Location(loc, __FILE__, __LINE__, #x), (x))
void impl_ensure(const Location& loc, bool res);

#define ensure_equals(x, y) wibble::tests::impl_ensure_equals(wibble::tests::Location(__FILE__, __LINE__, #x " == " #y), (x), (y))
#define inner_ensure_equals(x, y) wibble::tests::impl_ensure_equals(wibble::tests::Location(loc, __FILE__, __LINE__, #x " == " #y), (x), (y))

template <class Actual,class Expected>
void impl_ensure_equals(const Location& loc, const Actual& actual, const Expected& expected)
{
	if( expected != actual )
	{
		std::stringstream ss;
		ss << "expected '" << expected << "' actual '" << actual << "'";
        loc.fail_test(ss.str());
    }
}

#define ensure_similar(x, y, prec) wibble::tests::impl_ensure_similar(wibble::tests::Location(__FILE__, __LINE__, #x " == " #y), (x), (y), (prec))
#define inner_ensure_similar(x, y, prec) wibble::tests::impl_ensure_similar(wibble::tests::Location(loc, __FILE__, __LINE__, #x " == " #y), (x), (y), (prec))

template <class Actual, class Expected, class Precision>
void impl_ensure_similar(const Location& loc, const Actual& actual, const Expected& expected, const Precision& precision)
{
	if( actual < expected - precision || expected + precision < actual )
	{
		std::stringstream ss;
		ss << "expected '" << expected << "' actual '" << actual << "'";
        loc.fail_test(ss.str());
    }
}

#define ensure_contains(x, y) wibble::tests::impl_ensure_contains(wibble::tests::Location(__FILE__, __LINE__, #x " == " #y), (x), (y))
#define inner_ensure_contains(x, y) wibblwibblempl_ensure_contains(wibble::tests::Location(loc, __FILE__, __LINE__, #x " == " #y), (x), (y))
void impl_ensure_contains(const wibble::tests::Location& loc, const std::string& haystack, const std::string& needle);

#define ensure_not_contains(x, y) wibble::tests::impl_ensure_not_contains(wibble::tests::Location(__FILE__, __LINE__, #x " == " #y), (x), (y))
#define inner_ensure_not_contains(x, y) wibble::tests::impl_ensure_not_contains(wibble::tests::Location(loc, __FILE__, __LINE__, #x " == " #y), (x), (y))
void impl_ensure_not_contains(const wibble::tests::Location& loc, const std::string& haystack, const std::string& needle);


void test_assert_istrue(WIBBLE_TEST_LOCPRM, bool val);

template <class Expected, class Actual>
void test_assert_equals(WIBBLE_TEST_LOCPRM, const Expected& expected, const Actual& actual)
{
    if (expected != actual)
    {
        std::stringstream ss;
        ss << "expected '" << expected << "' actual '" << actual << "'";
        wibble_test_location.fail_test(ss.str());
    }
}

template <class Expected, class Actual>
void test_assert_gt(WIBBLE_TEST_LOCPRM, const Expected& expected, const Actual& actual)
{
    if (actual > expected) return;
    std::stringstream ss;
    ss << "value '" << actual << "' not greater than the expected '" << expected << "'";
    wibble_test_location.fail_test(ss.str());
}

template <class Expected, class Actual>
void test_assert_gte(WIBBLE_TEST_LOCPRM, const Expected& expected, const Actual& actual)
{
    if (actual >= expected) return;
    std::stringstream ss;
    ss << "value '" << actual << "' not greater or equal than the expected '" << expected << "'";
    wibble_test_location.fail_test(ss.str());
}

template <class Expected, class Actual>
void test_assert_lt(WIBBLE_TEST_LOCPRM, const Expected& expected, const Actual& actual)
{
    if (actual < expected) return;
    std::stringstream ss;
    ss << "value '" << actual << "' not less than the expected '" << expected << "'";
    wibble_test_location.fail_test(ss.str());
}

template <class Expected, class Actual>
void test_assert_lte(WIBBLE_TEST_LOCPRM, const Expected& expected, const Actual& actual)
{
    if (actual <= expected) return;
    std::stringstream ss;
    ss << "value '" << actual << "' not less than or equal than the expected '" << expected << "'";
    wibble_test_location.fail_test(ss.str());
}

void test_assert_startswith(WIBBLE_TEST_LOCPRM, const std::string& expected, const std::string& actual);
void test_assert_endswith(WIBBLE_TEST_LOCPRM, const std::string& expected, const std::string& actual);
void test_assert_contains(WIBBLE_TEST_LOCPRM, const std::string& expected, const std::string& actual);
void test_assert_re_match(WIBBLE_TEST_LOCPRM, const std::string& regexp, const std::string& actual);
void test_assert_file_exists(WIBBLE_TEST_LOCPRM, const std::string& fname);
void test_assert_not_file_exists(WIBBLE_TEST_LOCPRM, const std::string& fname);

#define test_runner(loc, func, ...) \
    do { try { \
        func(loc, ##__VA_ARGS__); \
    } catch (wibble::exception::Generic& e) { \
        loc.fail_test(e.what()); \
    } } while(0)

// wibble::tests::test_assert_* test
#define wtest(test, ...) test_runner(wibble_test_location.nest(wibble_test_location_info, __FILE__, __LINE__, #test ": " #__VA_ARGS__), wibble::tests::test_assert_##test, ##__VA_ARGS__)

// function test, just runs the function without mangling its name
#define ftest(test, ...) test_runner(wibble_test_location.nest(wibble_test_location_info, __FILE__, __LINE__, "function: " #test "(" #__VA_ARGS__ ")"), test, ##__VA_ARGS__)


}
}

#endif
