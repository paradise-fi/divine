/* -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>
               (c) 2007 Enrico Zini <enrico@enricozini.org> */
#include <wibble/sys/lockfile.h>
#include <cstdlib>
#include <set>

#include <wibble/test.h>

using namespace std;
using namespace wibble::sys::fs;

struct TestLockfile {
    Test readlock() {
		{
			Lockfile lk1("test", false);
			Lockfile lk2("test", false);
			try {
				Lockfile lk3("test", true);
				assert(false);
			} catch (...) {
			}
		}
		Lockfile lk3("test", true);
    }

    Test writelock() {
		{
			Lockfile lk1("test", true);
			try {
				Lockfile lk2("test", true);
				assert(false);
			} catch (...) {
			}
		}
		Lockfile lk2("test", true);
    }
};

// vim:set ts=4 sw=4:
