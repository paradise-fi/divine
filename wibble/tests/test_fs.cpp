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

// Test mkpath and friends
template<> template<>
void to::test< 3 >() {
	// Mkpath should succeed on existing directory
	mkpath(".");

	// Mkpath should succeed on existing directory
	mkpath("./.");

	// Mkpath should succeed on existing directory
	mkpath("/");
}

template<> template<>
void to::test< 4 >() {
	// Try creating a path with mkpath
	system("rm -rf test-mkpath");
	mkpath("test-mkpath/test-mkpath");
	ensure(wibble::sys::fs::access("test-mkpath", F_OK));
	ensure(wibble::sys::fs::access("test-mkpath/test-mkpath", F_OK));
	system("rm -rf test-mkpath");
}

template<> template<>
void to::test< 5 >() {
	// Try creating a path with mkFilePath
	system("rm -rf test-mkpath");
	mkFilePath("test-mkpath/test-mkpath/file");
	ensure(wibble::sys::fs::access("test-mkpath", F_OK));
	ensure(wibble::sys::fs::access("test-mkpath/test-mkpath", F_OK));
	ensure(!wibble::sys::fs::access("test-mkpath/test-mkpath/file", F_OK));
	system("rm -rf test-mkpath");
}


}

// vim:set ts=4 sw=4:
