#include <wibble/config.h>
#include <wibble/empty.h>
using namespace std;
using namespace wibble;

#ifdef WIBBLE_COMPILE_TESTSUITE
#include <wibble/tests/tut-wibble.h>

namespace tut {

struct empty_shar {};
TESTGRP( empty );

template<> template<>
void to::test< 1 >() {
    Empty<int> container;

    ensure_equals(container.size(), 0u);

    Empty<int>::iterator i = container.begin();
    ensure(i == container.end());
    ensure(!(i != container.end()));
}

}

#endif
