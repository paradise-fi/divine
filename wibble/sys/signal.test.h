/* -*- C++ -*- (c) 2009 Enrico Zini <enrico@enricozini.org> */
#include <wibble/sys/signal.h>
#include <set>
#include <cstdlib>

#include <wibble/test.h>

using namespace std;
using namespace wibble::sys::sig;

/*
 * Any ideas on how I can nicely test signal handling, since the default action
 * of a signal is to kill the process?
 *
 * Is there anything better than creating a child process and interacting with
 * it via pipes?

struct TestSignal {

    // Test directory iteration
    Test directoryIterate() {
        Directory dir("/");

        set<string> files;
        for (Directory::const_iterator i = dir.begin(); i != dir.end(); ++i)
            files.insert(*i);
    
        assert(files.size() > 0);
        assert(files.find(".") != files.end());
        assert(files.find("..") != files.end());
        assert(files.find("etc") != files.end());
        assert(files.find("usr") != files.end());
        assert(files.find("tmp") != files.end());
    }
};
*/

// vim:set ts=4 sw=4:
