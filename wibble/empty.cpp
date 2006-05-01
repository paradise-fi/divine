#include <wibble/config.h>
#include <wibble/empty.h>
using namespace std;
using namespace wibble;

#ifdef WIBBLE_COMPILE_TESTSUITE
#include <wibble/tests.h>

namespace wibble {
namespace tut {

struct empty_shar {};
TESTGRP( empty );

template<> template<>
void to::test< 1 >() {
    Empty container;

    ensure_equals(container.size(), 0u);

    Empty::iterator i = container.begin();
    ensure(i == container.end());
}

}
}

#endif
