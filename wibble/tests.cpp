/*
 * @file test-utils.cpp
 * @author Enrico Zini <enrico@enricozini.org>, Peter Rockai (mornfall) <me@mornfall.net>
 * @brief Utility functions for the unit tests
 *
 * Copyright (C) 2003--2013  Enrico Zini <enrico@debian.org>
 */

#include <wibble/tests.h>
#include <wibble/regexp.h>
#include <wibble/sys/fs.h>

using namespace std;
using namespace wibble;

const wibble::tests::Location wibble_test_location;
const wibble::tests::LocationInfo wibble_test_location_info;

namespace wibble {
namespace tests {

Location::Location()
    : parent(0), info(0), file(0), line(0), args(0)
{
    // Build a special, root location
}

Location::Location(const Location* parent, const wibble::tests::LocationInfo& info, const char* file, int line, const char* args)
    : parent(parent), info(&info), file(file), line(line), args(args)
{
}

Location::Location(const char* file, int line, const char* args)
    : parent(&wibble_test_location), info(&wibble_test_location_info), file(file), line(line), args(args)
{
}

Location::Location(const Location& parent, const char* file, int line, const char* args)
    : parent(&parent), info(&wibble_test_location_info), file(file), line(line), args(args)
{
}

Location Location::nest(const wibble::tests::LocationInfo& info, const char* file, int line, const char* args) const
{
    return Location(this, info, file, line, args);
}

void Location::backtrace(std::ostream& out) const
{
    if (parent) parent->backtrace(out);
    if (!file) return; // Root node, nothing to print
    out << file << ":" << line << ":" << args;
    if (!info->str().empty())
        out << " [" << info->str() << "]";
    out << endl;
}

std::string Location::locstr() const
{
    std::stringstream ss;
    backtrace(ss);
    return ss.str();
}

std::string Location::msg(const std::string msg) const
{
    std::stringstream ss;
    ss << "Test failed at:" << endl;
    backtrace(ss);
    ss << file << ":" << line << ":error: " << msg << endl;
    return ss.str();
}

void Location::fail_test(const std::string& msg) const
{
    throw tut::failure(this->msg(msg));
}

void Location::fail_test(const wibble::tests::LocationInfo& info, const char* file, int line, const char* args, const std::string& msg) const
{
    Location loc = nest(info, file, line, args);
    loc.fail_test(msg);
}

std::ostream& LocationInfo::operator()()
{
    str(std::string());
    clear();
    return *this;
}


void test_assert_re_match(WIBBLE_TEST_LOCPRM, const std::string& regexp, const std::string& actual)
{
    ERegexp re(regexp);
    if (!re.match(actual))
    {
        std::stringstream ss;
        ss << "'" << actual << "' does not match regexp '" << regexp << "'";
        wibble_test_location.fail_test(ss.str());
    }
}

void test_assert_startswith(WIBBLE_TEST_LOCPRM, const std::string& expected, const std::string& actual)
{
    if (!str::startsWith(actual, expected))
    {
        std::stringstream ss;
        ss << "'" << actual << "' does not start with '" << expected << "'";
        wibble_test_location.fail_test(ss.str());
    }
}

void test_assert_endswith(WIBBLE_TEST_LOCPRM, const std::string& expected, const std::string& actual)
{
    if (!str::endsWith(actual, expected))
    {
        std::stringstream ss;
        ss << "'" << actual << "' does not end with '" << expected << "'";
        wibble_test_location.fail_test(ss.str());
    }
}

void test_assert_contains(WIBBLE_TEST_LOCPRM, const std::string& expected, const std::string& actual)
{
    if (actual.find(expected) == string::npos)
    {
        std::stringstream ss;
        ss << "'" << actual << "' does not contain '" << expected << "'";
        wibble_test_location.fail_test(ss.str());
    }
}

void test_assert_istrue(WIBBLE_TEST_LOCPRM, bool val)
{
    if (!val)
        wibble_test_location.fail_test("result is false");
}

void test_assert_file_exists(WIBBLE_TEST_LOCPRM, const std::string& fname)
{
    if (not sys::fs::exists(fname))
    {
        std::stringstream ss;
        ss << "file '" << fname << "' does not exists";
        wibble_test_location.fail_test(ss.str());
    }
}

void test_assert_not_file_exists(WIBBLE_TEST_LOCPRM, const std::string& fname)
{
    if (sys::fs::exists(fname))
    {
        std::stringstream ss;
        ss << "file '" << fname << "' does exists";
        wibble_test_location.fail_test(ss.str());
    }
}

void impl_ensure(const Location& loc, bool res)
{
    if (!res)
        loc.fail_test("assertion failed");
}

void impl_ensure_contains(const wibble::tests::Location& loc, const std::string& haystack, const std::string& needle)
{
    if( haystack.find(needle) == std::string::npos )
    {
        std::stringstream ss;
        ss << "'" << haystack << "' does not contain '" << needle << "'";
        loc.fail_test(ss.str());
    }
}

void impl_ensure_not_contains(const wibble::tests::Location& loc, const std::string& haystack, const std::string& needle)
{
    if( haystack.find(needle) != std::string::npos )
    {
        std::stringstream ss;
        ss << "'" << haystack << "' must not contain '" << needle << "'";
        loc.fail_test(ss.str());
    }
}

void TestStartsWith::check(WIBBLE_TEST_LOCPRM) const
{
    if (!inverted)
    {
        if (str::startsWith(actual, expected)) return;
        std::stringstream ss;
        ss << "'" << actual << "' does not start with '" << expected << "'";
        wibble_test_location.fail_test(ss.str());
    } else {
        if (!str::startsWith(actual, expected)) return;
        std::stringstream ss;
        ss << "'" << actual << "' starts with '" << expected << "'";
        wibble_test_location.fail_test(ss.str());
    }
}

void TestEndsWith::check(WIBBLE_TEST_LOCPRM) const
{
    if (!inverted)
    {
        if (str::endsWith(actual, expected)) return;
        std::stringstream ss;
        ss << "'" << actual << "' does not end with '" << expected << "'";
        wibble_test_location.fail_test(ss.str());
    } else {
        if (!str::endsWith(actual, expected)) return;
        std::stringstream ss;
        ss << "'" << actual << "' ends with '" << expected << "'";
        wibble_test_location.fail_test(ss.str());
    }
}

void TestContains::check(WIBBLE_TEST_LOCPRM) const
{
    if (!inverted)
    {
        if (actual.find(expected) != std::string::npos) return;
        std::stringstream ss;
        ss << "'" << actual << "' does not contain '" << expected << "'";
        wibble_test_location.fail_test(ss.str());
    } else {
        if (actual.find(expected) == std::string::npos) return;
        std::stringstream ss;
        ss << "'" << actual << "' contains '" << expected << "'";
        wibble_test_location.fail_test(ss.str());
    }
}

void TestRegexp::check(WIBBLE_TEST_LOCPRM) const
{
    ERegexp re(regexp);
    if (!inverted)
    {
        if (re.match(actual)) return;
        std::stringstream ss;
        ss << "'" << actual << "' does not match regexp '" << regexp << "'";
        wibble_test_location.fail_test(ss.str());
    } else {
        if (!re.match(actual)) return;
        std::stringstream ss;
        ss << "'" << actual << "' matches regexp '" << regexp << "'";
        wibble_test_location.fail_test(ss.str());
    }
}

void TestFileExists::check(WIBBLE_TEST_LOCPRM) const
{
    if (!inverted)
    {
        if (sys::fs::exists(pathname)) return;
        std::stringstream ss;
        ss << "file '" << pathname << "' does not exists";
        wibble_test_location.fail_test(ss.str());
    } else {
        if (not sys::fs::exists(pathname)) return;
        std::stringstream ss;
        ss << "file '" << pathname << "' exists";
        wibble_test_location.fail_test(ss.str());
    }
}

}
}

// vim:set ts=4 sw=4:
