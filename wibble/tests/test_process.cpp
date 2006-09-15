#include <wibble/config.h>
#include <wibble/sys/process.h>
using namespace std;
using namespace wibble::sys;

#include <wibble/tests/tut-wibble.h>

namespace tut {

struct sys_process_shar {};
TESTGRP( sys_process );

// Test chdir and getcwd
template<> template<>
void to::test< 1 >() {
	string cwd = process::getcwd();
	process::chdir("/");
	ensure_equals(process::getcwd(), string("/"));
	process::chdir(cwd);
	ensure_equals(process::getcwd(), cwd);
}

// Test umask
template<> template<>
void to::test< 2 >() {
	mode_t old = process::umask(0012);
	ensure_equals(process::umask(old), 0012u);
}

}

// vim:set ts=4 sw=4:
