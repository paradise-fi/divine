/* -*- C++ -*-
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

#include <wibble/test.h>
#include <wibble/exception.h>
#include <errno.h>

namespace {

using namespace std;

struct TestException {
    Test generic()
    {
        try {
            throw wibble::exception::Generic("antani");
        } catch ( std::exception& e ) {
            assert(string(e.what()).find("antani") != string::npos);
        }
        
        try {
            throw wibble::exception::Generic("antani");
        } catch ( wibble::exception::Generic& e ) {
            assert(e.fullInfo().find("antani") != string::npos);
        }
    }

    Test system() 
    {
        try {
            assert_eq(access("does-not-exist", F_OK), -1);
            throw wibble::exception::System("checking for existance of nonexisting file");
        } catch ( wibble::exception::System& e ) {
            // Check that we caught the right value of errno
            assert_eq(e.code(), ENOENT);
        }
        
        try {
            assert_eq(access("does-not-exist", F_OK), -1);
            throw wibble::exception::File("does-not-exist", "checking for existance of nonexisting file");
        } catch ( wibble::exception::File& e ) {
            // Check that we caught the right value of errno
            assert_eq(e.code(), ENOENT);
            assert(e.fullInfo().find("does-not-exist") != string::npos);
        }
    }

    Test badCast()
    {
        int check = -1;
        try {
            check = 0;
            throw wibble::exception::BadCastExt< int, const char * >( "test" );
            check = 1;
        } catch ( wibble::exception::BadCast& e ) {
            assert_eq( e.fullInfo(), "bad cast: from i to PKc. Context: test" );
            check = 2;
        }
        assert_eq( check, 2 );
    }
};

}
// vim:set ts=4 sw=4:
