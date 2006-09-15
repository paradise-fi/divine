#include <wibble/config.h>
#include <wibble/sys/fs.h>
#include <set>

using namespace std;
using namespace wibble::sys::fs;

#include <wibble/tests/tut-wibble.h>

namespace tut {

struct sys_fs_shar {};
TESTGRP( sys_fs );

// Test directory iteration
template<> template<>
void to::test< 1 >() {
	Directory dir("/");

	set<string> files;
	for (Directory::const_iterator i = dir.begin(); i != dir.end(); ++i)
		files.insert(*i);

	ensure(files.size() > 0);
	ensure(files.find(".") != files.end());
	ensure(files.find("..") != files.end());
	ensure(files.find("etc") != files.end());
	ensure(files.find("usr") != files.end());
	ensure(files.find("tmp") != files.end());
}

// Ensure that nonexisting directories and files are reported as not valid
template<> template<>
void to::test< 2 >() {
	Directory dir1("/antaniblindalasupercazzola123456");
	ensure(!dir1.valid());

	Directory dir2("/etc/passwd");
	ensure(!dir2.valid());
}

}

// vim:set ts=4 sw=4:
