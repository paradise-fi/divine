// -*- C++ -*-
#ifndef WIBBLE_SINGLETON_H
#define WIBBLE_SINGLETON_H

/*
 * Degenerated container to hold a single value
 *
 * Copyright (C) 2006  Enrico Zini <enrico@debian.org>
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

namespace wibble {

template<typename T>
class Singleton
{
protected:
	T value;
	
public:
	typedef T value_type;

	class iterator
	{
		T* value;

	protected:
		iterator(T* value) : value(value) {}
	
	public:
		iterator() : value(0) {}

		T& operator*() const { return *value; }
		iterator& operator++() { value = 0; return *this; }
		bool operator==(const iterator& iter) { return value == iter.value; }
		bool operator!=(const iterator& iter) { return value != iter.value; }
		
		friend class Singleton<T>;
	};
	
	Singleton(const T& value) : value(value) {}

	unsigned int size() const { return 1; }

	iterator begin() { return iterator(&value); }
	iterator end() { return iterator(); }
};

template<typename T>
Singleton<T> singleton(const T& value)
{
	return Singleton<T>(value);
}

}

// vim:set ts=4 sw=4:
#endif
