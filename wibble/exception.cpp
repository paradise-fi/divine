/*
 * Generic base exception hierarchy
 * 
 * Copyright (C) 2003  Enrico Zini <enrico@debian.org>
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
#include <Exception.h>

#include <string.h> // strerror_r
#include <errno.h> // strerror_r

#include <typeinfo>
#include <sstream>

#include <execinfo.h>

using namespace std;

void DefaultUnexpected()
{
	try {
		const int trace_size = 50;
		void *addrs[trace_size];
		size_t size = backtrace (addrs, trace_size);
		char **strings = backtrace_symbols (addrs, size);

		fprintf(stderr, "Caught unexpected exception, %d stack frames unwound:\n", (int)size);
		for (size_t i = 0; i < size; i++)
			fprintf (stderr, "   %s\n", strings[i]);
		free (strings);
		throw;
	} catch (Exception& e) {
		fprintf(stderr, "Exception was: %s: %.*s.\n", e.type(), PFSTR(e.desc()));
		throw;
	} catch (exception& e) {
		fprintf(stderr, "Exception was: %s: %s\n", typeid(e).name(), e.what());
		throw;
	} catch (...) {
		fprintf(stderr, "Exception was: unknown object\n");
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
virtual std::string ValOutOfRange<C>::desc() const throw ()
{
	stringstream str;
	str << m_var_desc << "(" << m_val << ") out of range (" <<
			_inf << "-" << _sup << ")";
	return str.str();
}

///// SystemException

System::System(const std::string& context) throw ()
	: Generic(context), m_errno(errno) {}

string SystemException::desc() const throw ()
{
	const int buf_size = 100;
	char buf[buf_size];
	strerror_r(_code, buf, buf_size);
	return buf;
}

// vim:set ts=4 sw=4:
