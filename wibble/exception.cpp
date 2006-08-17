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

System::System(int code, const std::string& context) throw ()
	: Generic(context), m_errno(code) {}

string System::desc() const throw ()
{
	// FIXME: this use of strerror_r is broken on non-GNU systems
	const int buf_size = 500;
	char buf[buf_size];
	char* res;
	res = strerror_r(m_errno, buf, buf_size);
	buf[buf_size - 1] = 0;
	return string(res);
}

}
}

// vim:set ts=4 sw=4:
