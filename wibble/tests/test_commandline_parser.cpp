#include <wibble/config.h>
#include <wibble/commandline/parser.h>
#include <wibble/tests/tut-wibble.h>

namespace tut {

struct commandline_parser_shar {
};
TESTGRP( commandline_parser );

using namespace wibble::commandline;

template<> template<>
void to::test<1>()
{
}

}

// vim:set ts=4 sw=4:
