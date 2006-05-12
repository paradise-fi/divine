/*
 * Generic base exception hierarchy
 * 
 * Copyright (C) 2003,2004,2005,2006  Enrico Zini <enrico@debian.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */
#include <wibble/config.h>
#include <wibble/exception.h>

#include <string.h> // strerror_r
#include <errno.h>

#include <typeinfo>
#include <sstream>
#include <iostream>

#include <execinfo.h>

using namespace std;

namespace wibble {
namespace exception {

void DefaultUnexpected()
{
	try {
		const int trace_size = 50;
		void *addrs[trace_size];
		size_t size = backtrace (addrs, trace_size);
		char **strings = backtrace_symbols (addrs, size);

		cerr << "Caught unexpected exception, " << size << " stack frames unwound:" << endl;
		for (size_t i = 0; i < size; i++)
			cerr << "   " << strings[i] << endl;
		free (strings);
		throw;
	} catch (Generic& e) {
		cerr << "Exception was: " << e.type() << ": " << e.fullInfo() << endl;
		throw;
	} catch (std::exception& e) {
		cerr << "Exception was: " << typeid(e).name() << ": " << e.what() << endl;
		throw;
	} catch (...) {
		cerr << "Exception was an unknown object" << endl;
		throw;
	}
}

InstallUnexpected::InstallUnexpected(void (*func)())
{
	old = set_unexpected(func);
}

InstallUnexpected::~InstallUnexpected()
{
	set_unexpected(old);
}


template<typename C>
std::string ValOutOfRange<C>::desc() const throw ()
{
	stringstream str;
	str << m_var_desc << "(" << m_val << ") out of range (" <<
			m_inf << "-" << m_sup << ")";
	return str.str();
}

///// SystemException

System::System(const std::string& context) throw ()
	: Generic(context), m_errno(errno) {}

string System::desc() const throw ()
{
	const int buf_size = 500;
	char buf[buf_size];
	char* res;
	res = strerror_r(m_errno, buf, buf_size);
	buf[buf_size - 1] = 0;
	return string(res);
}

}
}

#ifdef WIBBLE_COMPILE_TESTSUITE

#include <wibble/tests.h>
#include <unistd.h>

namespace wibble {
namespace tut {

using namespace tests;

struct exception_shar {
};

TESTGRP(exception);

// Generic
template<> template<>
void to::test<1>()
{
	try {
		throw exception::Generic("antani");
	} catch ( std::exception& e ) {
		ensure(string(e.what()).find("antani") != string::npos);
	}

	try {
		throw exception::Generic("antani");
	} catch ( exception::Generic& e ) {
		ensure(e.fullInfo().find("antani") != string::npos);
	}
}

// System
template<> template<>
void to::test<2>()
{
	try {
		ensure_equals(access("does-not-exist", F_OK), -1);
		throw exception::System("checking for existance of nonexisting file");
	} catch ( exception::System& e ) {
		// Check that we caught the right value of errno
		ensure_equals(e.code(), ENOENT);
	}

	try {
		ensure_equals(access("does-not-exist", F_OK), -1);
		throw exception::File("does-not-exist", "checking for existance of nonexisting file");
	} catch ( exception::File& e ) {
		// Check that we caught the right value of errno
		ensure_equals(e.code(), ENOENT);
		ensure(e.fullInfo().find("does-not-exist") != string::npos);
	}
}

// BadCast
template<> template<>
void to::test<3>()
{
    int check = -1;
	try {
        check = 0;
		throw exception::BadCastExt< int, const char * >( "test" );
        check = 1;
	} catch ( exception::BadCast& e ) {
        ensure_equals( e.fullInfo(), "bad cast: from i to PKc. Context: test" );
        check = 2;
	}
    ensure_equals( check, 2 );
}

}

}

#endif
// vim:set ts=4 sw=4:
