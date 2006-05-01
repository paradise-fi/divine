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
	class iterator
	{
		T* value;

	protected:
		Iterator(T* value) : value(value) {}
	
	public:
		Iterator() : value(0) {}

		T* operator*() const { return *value; }
		Iterator& operator++() { value = 0; }
	};
	
	Singleton(const T& value) : value(value) {}

	int size() const { return 1; }

	iterator begin() const { return Iterator(&value); }
	iterator end() const { return Iterator(); }
}:

template<typename T>
Singleton<T> singleton(const T& value)
{
	return Singleton<T>(value);
}

}

// vim:set ts=4 sw=4:
#endif
