// -*- C++ -*-
#ifndef WIBBLE_EMPTY_H
#define WIBBLE_EMPTY_H

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

class Empty
{
public:
	class iterator
	{
	public:
		template<typename T>
		T& operator*() const { return *(T*)0; }
		Iterator& operator++() const {}
	};
	
	int size() const { return 0; }

	iterator begin() const { return Iterator(); }
	iterator end() const { return Iterator(); }
}:

}

// vim:set ts=4 sw=4:
#endif
