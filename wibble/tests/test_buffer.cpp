#include <wibble/config.h>
#include <wibble/sys/buffer.h>

using namespace std;
using namespace wibble::sys;

#include <wibble/tests/tut-wibble.h>

namespace tut {

struct sys_buffer_shar {};
TESTGRP( sys_buffer );

// Empty buffers should be properly empty
template<> template<>
void to::test< 1 >() {
	Buffer buf;
	ensure_equals(buf.size(), 0u);
	ensure_equals(buf.data(), (void*)0);
}

// Nonempty buffers should be properly nonempty
template<> template<>
void to::test< 2 >() {
	Buffer buf(1);
	ensure_equals(buf.size(), 1u);
	ensure(buf.data() != 0);
}

// Construct by copy should work
template<> template<>
void to::test< 3 >() {
	const char* str = "Ciao";
	Buffer buf(str, 4);

	ensure_equals(buf.size(), 4u);
	ensure(memcmp(str, buf.data(), 4) == 0);
}

// Resize should work and preserve the contents
template<> template<>
void to::test< 4 >() {
	const char* str = "Ciao";
	Buffer buf(str, 4);

	ensure_equals(buf.size(), 4u);
	ensure(memcmp(str, buf.data(), 4) == 0);

	buf.resize(8);
	ensure_equals(buf.size(), 8u);
	ensure(memcmp(str, buf.data(), 4) == 0);
}

// Check creation by taking ownership of another buffer
template<> template<>
void to::test< 5 >() {
	char* str = new char[4];
	memcpy(str, "ciao", 4);
	Buffer buf(str, 4, true);
	
	ensure_equals(buf.size(), 4u);
	ensure_equals((void*)str, buf.data());
}


}

// vim:set ts=4 sw=4:
