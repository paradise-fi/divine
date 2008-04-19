/*
 * Simple mmap support
 *
 * Copyright (C) 2006--2008  Enrico Zini <enrico@debian.org>
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

#include <wibble/sys/mmap.h>
#include <wibble/test.h>
#include <string.h>

using namespace std;
using namespace wibble::sys;

struct TestMMap {
    Test simple() {
        MMap map;
        assert_eq(map.filename, string());
        assert_eq(map.fd, -1);
        assert_eq(map.size, 0u);
        assert_eq(map.buf, (const char*)0);

    	map.map("/bin/ls");
        assert_eq(map.filename, "/bin/ls");
        assert(map.fd != -1);
        assert(map.size != 0u);
        assert(map.buf != (const char*)0);
    	assert_eq(map.buf[1], 'E');
    	assert_eq(map.buf[2], 'L');
    	assert_eq(map.buf[3], 'F');

	map.unmap();
        assert_eq(map.filename, string());
        assert_eq(map.fd, -1);
        assert_eq(map.size, 0u);
        assert_eq(map.buf, (const char*)0);
    }
};

// vim:set ts=4 sw=4:
