#include <wibble/config.h>
#include <wibble/sys/netbuffer.h>

using namespace std;
using namespace wibble::sys;

#include <wibble/tests/tut-wibble.h>

namespace tut {

struct sys_netbuffer_shar {};
TESTGRP( sys_netbuffer );

// Ensure that a plain NetBuffer starts from the beginning of the string
template<> template<>
void to::test< 1 >() {
	NetBuffer buf("antani", 6);

	ensure_equals(buf.size(), 6u);
	ensure(memcmp(buf.data(), "antani", 6) == 0);

	ensure_equals(*buf.cast<char>(), 'a');
	ensure(buf.fits<short int>());
	ensure(!buf.fits<long long int>());
}

// Try skipping some bytes
template<> template<>
void to::test< 2 >() {
	NetBuffer buf("antani", 6);

	NetBuffer buf1(buf);

	ensure_equals(buf1.size(), 6u);
	ensure(memcmp(buf1.data(), "antani", 6) == 0);

	buf1 += 2;
	ensure_equals(buf1.size(), 4u);
	ensure(memcmp(buf1.data(), "tani", 4) == 0);
	ensure_equals(*buf1.cast<char>(), 't');

	buf1 = buf + 2;
	ensure_equals(buf1.size(), 4u);
	ensure(memcmp(buf1.data(), "tani", 4) == 0);
	ensure_equals(*buf1.cast<char>(), 't');

	buf1 = buf;
	buf1.skip(2);
	ensure_equals(buf1.size(), 4u);
	ensure(memcmp(buf1.data(), "tani", 4) == 0);
	ensure_equals(*buf1.cast<char>(), 't');

	buf1 = buf.after(2);
	ensure_equals(buf1.size(), 4u);
	ensure(memcmp(buf1.data(), "tani", 4) == 0);
	ensure_equals(*buf1.cast<char>(), 't');

	buf1 = buf;
	buf1.skip<short int>();
	ensure_equals(buf1.size(), 4u);
	ensure(memcmp(buf1.data(), "tani", 4) == 0);
	ensure_equals(*buf1.cast<char>(), 't');

	buf1 = buf.after<short int>();
	ensure_equals(buf1.size(), 4u);
	ensure(memcmp(buf1.data(), "tani", 4) == 0);
	ensure_equals(*buf1.cast<char>(), 't');
}

}

// vim:set ts=4 sw=4:
