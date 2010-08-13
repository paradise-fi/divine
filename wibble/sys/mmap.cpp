/*
 * Simple mmap support
 *
 * Copyright (C) 2006--2008  Enrico Zini <enrico@enricozini.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <wibble/exception.h>
#include <wibble/sys/mmap.h>

#ifdef POSIX
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

using namespace std;

namespace wibble {
namespace sys {

MMap::MMap() : size(0), fd(-1), buf(0) {}

MMap::MMap(const std::string& filename)
	: size(0), fd(-1), buf(0)
{
	// If map throws here, the destructor isn't called (since we're in the
	// constructor), so we do the cleanup and rethrow.
	try {
		map(filename);
	} catch (...) {
		unmap();
		throw;
	}
}

MMap::MMap(const MMap& mmap)
	: filename(mmap.filename), size(mmap.size), fd(mmap.fd), buf(mmap.buf)
{
	// Cast away const to have auto_ptr semantics
	MMap* wm = const_cast<MMap*>(&mmap);
	wm->filename.clear();
	wm->size = 0;
	wm->fd = -1;
	wm->buf = 0;
}

MMap& MMap::operator=(const MMap& mmap)
{
	// Handle assignment to self
	if (this == &mmap)
		return *this;

	if (fd) unmap();

	filename = mmap.filename;
	size = mmap.size;
	fd = mmap.fd;
	buf = mmap.buf;

	// Cast away const to have auto_ptr semantics
	MMap* wm = const_cast<MMap*>(&mmap);
	wm->filename.clear();
	wm->size = 0;
	wm->fd = -1;
	wm->buf = 0;

	return *this;
}

MMap::~MMap()
{
	unmap();
}

void MMap::map(const std::string& filename)
{
	if (buf) unmap();

	try {
		this->filename = filename;

		// Open the file
		if ((fd = open(filename.c_str(), O_RDONLY)) == -1)
			throw wibble::exception::System("opening index file " + filename);

		size = lseek(fd, 0, SEEK_END);
		if (size == (off_t)-1)
			throw wibble::exception::System("reading the size of index file " + filename);
		if (size == 0)
			throw wibble::exception::Consistency("ensuring that there is data in the index",
										"the mmap index file " + filename + " is empty");

		// Map the file into memory
		if ((buf = (const char*)::mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
			throw wibble::exception::System("mmapping file " + filename);
	} catch (...) {
		unmap();
		throw;
	}
}

void MMap::unmap()
{
	// Unmap and close the file
	if (buf)
	{
		if (buf != MAP_FAILED)
			munmap((void*)buf, size);
		buf = 0;
		size = 0;
	}
	if (fd != -1)
	{
		close(fd);
		fd = -1;
	}
	filename.clear();
}

}
}
#endif
// vim:set ts=4 sw=4:
