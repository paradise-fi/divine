/*
 * Variable-size, reference-counted memory buffer
 *
 * Copyright (C) 2003--2006  Enrico Zini <enrico@debian.org>
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

#include <wibble/sys/buffer.h>

#include <string.h>	// memcpy
#include <stdlib.h>	// malloc, free, realloc

namespace wibble {
namespace sys {

Buffer::Data::Data(size_t size) : _ref(0), _size(size)
{
	_data = malloc(size);
}

Buffer::Data::Data(void* buf, size_t size, bool own)
	: _ref(0), _size(size)
{
	if (own)
		_data = buf;
	else
	{
		_data = malloc(size);
		memcpy(_data, buf, size);
	}
}

Buffer::Data::Data(const void* buf, size_t size)
	: _ref(0), _size(size)
{
	_data = malloc(size);
	memcpy(_data, buf, size);
}

Buffer::Data::~Data()
{
	if (_data)
		free(_data);
}
	

void Buffer::Data::resize(size_t size)
{
	if (size == 0)
	{
		if (_data)
		{
			free(_data);
			_data = 0;
		}
	} else if (_data == 0) {
		_data = malloc(size);
	} else {
		_data = realloc(_data, size);
	}
	_size = size;
}

/// Compare the contents of two buffers
bool Buffer::Data::operator==(const Data& d) const throw()
{
	if (_size != d._size)
		return false;
	if (_data == 0 && d._data == 0)
		return true;
	if (_data == 0 || d._data == 0)
		return false;
	return memcmp(_data, d._data, _size) == 0;
}

/// Compare the contents of two buffers
bool Buffer::Data::operator<(const Data& d) const throw()
{
	if (_size < d._size)
		return true;
	if (_size > d._size)
		return false;
	if (_data == 0 && d._data == 0)
		return false;
	if (_data == 0)
		return true;
	if (d._data == 0)
		return false;
	return memcmp(_data, d._data, _size) < 0;
}

}
}

// vim:set ts=4 sw=4:
