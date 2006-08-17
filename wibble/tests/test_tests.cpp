/*
 * Implementation of some test utility functions
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

#if 0
#include <wibble/config.h>
#include <wibble/tests/tut-wibble.h>
#endif

#include <wibble/tests.h>

namespace tut {

using namespace wibble::tests;

struct tests_shar {
};

TESTGRP(tests);

template<> template<>
void to::test<1>()
{
	ensure(true);
	ensure_equals(42, 42);
}

}

// vim:set ts=4 sw=4:
