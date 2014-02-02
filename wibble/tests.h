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

struct Location
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
    void fail_test(const wibble::tests::LocationInfo& info, const char* file, int line, const char* args, const std::string& msg) const WIBBLE_TESTS_ALWAYS_THROWS;
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
    LocationInfo() {}
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


template<typename A>
struct TestBool
{
    const A& actual;
    bool inverted;
    TestBool(const A& actual, bool inverted=false) : actual(actual), inverted(inverted) {}

    TestBool<A> operator!() { return TestBool(actual, !inverted); }

    void check(WIBBLE_TEST_LOCPRM) const
    {
        if (!inverted)
        {
            if (actual) return;
            wibble_test_location.fail_test("actual value is false");
        } else {
            if (!actual) return;
            wibble_test_location.fail_test("actual value is true");
        }
    }
};

template<typename A, typename E>
struct TestEquals
{
    A actual;
    E expected;
    bool inverted;
    TestEquals(const A& actual, const E& expected, bool inverted=false)
        : actual(actual), expected(expected), inverted(inverted) {}

    TestEquals<A, E> operator!() { return TestEquals<A, E>(actual, expected, !inverted); }
    void check(WIBBLE_TEST_LOCPRM) const
    {
        if (!inverted)
        {
            if (actual == expected) return;
            std::stringstream ss;
            ss << "value '" << actual << "' is different than the expected '" << expected << "'";
            wibble_test_location.fail_test(ss.str());
        } else {
            if (actual != expected) return;
            std::stringstream ss;
            ss << "value '" << actual << "' is not different than the expected '" << expected << "'";
            wibble_test_location.fail_test(ss.str());
        }
    }
};

template<typename A, typename E>
struct TestIsLt
{
    A actual;
    E expected;
    bool inverted;
    TestIsLt(const A& actual, const E& expected, bool inverted=false)
        : actual(actual), expected(expected), inverted(inverted) {}

    TestIsLt<A, E> operator!() { return TestIsLt<A, E>(actual, expected, !inverted); }
    void check(WIBBLE_TEST_LOCPRM) const
    {
        if (!inverted)
        {
            if (actual < expected) return;
            std::stringstream ss;
            ss << "value '" << actual << "' is not less than the expected '" << expected << "'";
            wibble_test_location.fail_test(ss.str());
        } else {
            if (!(actual < expected)) return;
            std::stringstream ss;
            ss << "value '" << actual << "' is less than the expected '" << expected << "'";
            wibble_test_location.fail_test(ss.str());
        }
    }
};

template<typename A, typename E>
struct TestIsLte
{
    A actual;
    E expected;
    bool inverted;
    TestIsLte(const A& actual, const E& expected, bool inverted=false) : actual(actual), expected(expected), inverted(inverted) {}

    TestIsLte<A, E> operator!() { return TestIsLte<A, E>(actual, expected, !inverted); }
    void check(WIBBLE_TEST_LOCPRM) const
    {
        if (!inverted)
        {
            if (actual <= expected) return;
            std::stringstream ss;
            ss << "value '" << actual << "' is not less than or equals to the expected '" << expected << "'";
            wibble_test_location.fail_test(ss.str());
        } else {
            if (!(actual <= expected)) return;
            std::stringstream ss;
            ss << "value '" << actual << "' is less than or equals to the expected '" << expected << "'";
            wibble_test_location.fail_test(ss.str());
        }
    }
};

template<typename A, typename E>
struct TestIsGt
{
    A actual;
    E expected;
    bool inverted;
    TestIsGt(const A& actual, const E& expected, bool inverted=false) : actual(actual), expected(expected), inverted(inverted) {}

    TestIsGt<A, E> operator!() { return TestIsGt<A, E>(actual, expected, !inverted); }
    void check(WIBBLE_TEST_LOCPRM) const
    {
        if (!inverted)
        {
            if (actual > expected) return;
            std::stringstream ss;
            ss << "value '" << actual << "' is not greater than the expected '" << expected << "'";
            wibble_test_location.fail_test(ss.str());
        } else {
            if (!(actual > expected)) return;
            std::stringstream ss;
            ss << "value '" << actual << "' is greater than the expected '" << expected << "'";
            wibble_test_location.fail_test(ss.str());
        }
    }
};

template<typename A, typename E>
struct TestIsGte
{
    A actual;
    E expected;
    bool inverted;
    TestIsGte(const A& actual, const E& expected, bool inverted=false) : actual(actual), expected(expected), inverted(inverted) {}

    TestIsGte<A, E> operator!() { return TestIsGte<A, E>(actual, expected, !inverted); }
    void check(WIBBLE_TEST_LOCPRM) const
    {
        if (!inverted)
        {
            if (actual >= expected) return;
            std::stringstream ss;
            ss << "value '" << actual << "' is not greater than or equals to the expected '" << expected << "'";
            wibble_test_location.fail_test(ss.str());
        } else {
            if (!(actual >= expected)) return;
            std::stringstream ss;
            ss << "value '" << actual << "' is greater than or equals to the expected '" << expected << "'";
            wibble_test_location.fail_test(ss.str());
        }
    }
};

struct TestStartsWith
{
    std::string actual;
    std::string expected;
    bool inverted;
    TestStartsWith(const std::string& actual, const std::string& expected, bool inverted=false) : actual(actual), expected(expected), inverted(inverted) {}

    TestStartsWith operator!() { return TestStartsWith(actual, expected, !inverted); }
    void check(WIBBLE_TEST_LOCPRM) const;
};

struct TestEndsWith
{
    std::string actual;
    std::string expected;
    bool inverted;
    TestEndsWith(const std::string& actual, const std::string& expected, bool inverted=false) : actual(actual), expected(expected), inverted(inverted) {}

    TestEndsWith operator!() { return TestEndsWith(actual, expected, !inverted); }
    void check(WIBBLE_TEST_LOCPRM) const;
};

struct TestContains
{
    std::string actual;
    std::string expected;
    bool inverted;
    TestContains(const std::string& actual, const std::string& expected, bool inverted=false) : actual(actual), expected(expected), inverted(inverted) {}

    TestContains operator!() { return TestContains(actual, expected, !inverted); }
    void check(WIBBLE_TEST_LOCPRM) const;
};

struct TestRegexp
{
    std::string actual;
    std::string regexp;
    bool inverted;
    TestRegexp(const std::string& actual, const std::string& regexp, bool inverted=false) : actual(actual), regexp(regexp), inverted(inverted) {}

    TestRegexp operator!() { return TestRegexp(actual, regexp, !inverted); }
    void check(WIBBLE_TEST_LOCPRM) const;
};

struct TestFileExists
{
    std::string pathname;
    bool inverted;
    TestFileExists(const std::string& pathname, bool inverted=false) : pathname(pathname), inverted(inverted) {}
    TestFileExists operator!() { return TestFileExists(pathname, !inverted); }
    void check(WIBBLE_TEST_LOCPRM) const;
};


template<class A>
struct Actual
{
    A actual;
    Actual(const A& actual) : actual(actual) {}
    ~Actual() {}

    template<typename E> TestEquals<A, E> operator==(const E& expected) const { return TestEquals<A, E>(actual, expected); }
    template<typename E> TestEquals<A, E> operator!=(const E& expected) const { return !TestEquals<A, E>(actual, expected); }
    template<typename E> TestIsLt<A, E> operator<(const E& expected) const { return TestIsLt<A, E>(actual, expected); }
    template<typename E> TestIsLte<A, E> operator<=(const E& expected) const { return TestIsLte<A, E>(actual, expected); }
    template<typename E> TestIsGt<A, E> operator>(const E& expected) const { return TestIsGt<A, E>(actual, expected); }
    template<typename E> TestIsGte<A, E> operator>=(const E& expected) const { return TestIsGte<A, E>(actual, expected); }
    TestBool<A> istrue() const { return TestBool<A>(actual); }
    TestBool<A> isfalse() const { return TestBool<A>(actual, true); }
};

struct ActualString : public Actual<std::string>
{
    ActualString(const std::string& s) : Actual<std::string>(s) {}
    TestEquals<std::string, std::string> operator==(const std::string& expected) const { return TestEquals<std::string, std::string>(actual, expected); }
    TestEquals<std::string, std::string> operator!=(const std::string& expected) const { return !TestEquals<std::string, std::string>(actual, expected); }
    TestIsLt<std::string, std::string> operator<(const std::string& expected) const { return TestIsLt<std::string, std::string>(actual, expected); }
    TestIsLte<std::string, std::string> operator<=(const std::string& expected) const { return TestIsLte<std::string, std::string>(actual, expected); }
    TestIsGt<std::string, std::string> operator>(const std::string& expected) const { return TestIsGt<std::string, std::string>(actual, expected); }
    TestIsGte<std::string, std::string> operator>=(const std::string& expected) const { return TestIsGte<std::string, std::string>(actual, expected); }
    TestStartsWith startswith(const std::string& expected) const { return TestStartsWith(actual, expected); }
    TestEndsWith endswith(const std::string& expected) const { return TestEndsWith(actual, expected); }
    TestContains contains(const std::string& expected) const { return TestContains(actual, expected); }
    TestRegexp matches(const std::string& regexp) const { return TestRegexp(actual, regexp); }
    TestFileExists fileexists() const { return TestFileExists(actual); }
};

template<typename A>
inline Actual<A> actual(const A& actual) { return Actual<A>(actual); }
inline ActualString actual(const std::string& actual) { return ActualString(actual); }
inline ActualString actual(const char* actual) { return ActualString(actual ? actual : ""); }
inline ActualString actual(char* actual) { return ActualString(actual ? actual : ""); }

/*
template<typename T, typename P>
void _wassert(WIBBLE_TEST_LOCPRM, T& a, P& op)
{
    op.invoke(wibble_test_location, a);
}
*/

template<typename T>
static inline void _wassert(WIBBLE_TEST_LOCPRM, const T& expr)
{
    expr.check(wibble_test_location);
}


#define wibble_test_runner(loc, func, ...) \
    do { try { \
        func(loc, ##__VA_ARGS__); \
    } catch (tut::failure) { \
        throw; \
    } catch (std::exception& e) { \
        loc.fail_test(e.what()); \
    } } while(0)

#define wrunchecked(func) \
    do { try { \
        func; \
    } catch (tut::failure) { \
        throw; \
    } catch (std::exception& e) { \
        wibble_test_location.fail_test(wibble_test_location_info, __FILE__, __LINE__, #func, e.what()); \
    } } while(0)

// function test, just runs the function without mangling its name
#define wruntest(test, ...) wibble_test_runner(wibble_test_location.nest(wibble_test_location_info, __FILE__, __LINE__, "function: " #test "(" #__VA_ARGS__ ")"), test, ##__VA_ARGS__)

#define wassert(...) wibble_test_runner(wibble_test_location.nest(wibble_test_location_info, __FILE__, __LINE__, #__VA_ARGS__), _wassert, ##__VA_ARGS__)

}
}

#endif
