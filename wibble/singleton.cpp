#include <wibble/config.h>
#include <wibble/singleton.h>
using namespace std;
using namespace wibble;

#ifdef WIBBLE_COMPILE_TESTSUITE
#include <wibble/tests/tut-wibble.h>

namespace tut {

struct singleton_shar {};
TESTGRP( singleton );

template<> template<>
void to::test< 1 >() {
    Singleton<int> container = singleton(5);

    ensure_equals(container.size(), 1u);

    Singleton<int>::iterator i = container.begin();
    ensure(!(i == container.end()));
    ensure(i != container.end());
    ensure_equals(*i, 5);

    ++i;
    ensure(i == container.end());
    ensure(!(i != container.end()));
}

}

#endif
