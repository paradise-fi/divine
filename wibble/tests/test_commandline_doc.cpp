#include <wibble/config.h>
#include <wibble/commandline/doc.h>
#include <wibble/tests/tut-wibble.h>

namespace tut {

struct wibble_commandline_shar {
};
TESTGRP(wibble_commandline);

using namespace wibble::commandline;

// FIXME: I don't know how to do tests for documentation --enrico
template<> template<>
void to::test<1>()
{
}

}

// vim:set ts=4 sw=4:
