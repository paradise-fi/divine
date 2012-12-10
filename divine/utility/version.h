// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <iostream>

// Set version information in version.cpp.

#ifndef DIVINE_VERSION_H
#define DIVINE_VERSION_H

extern const char* divineCompileFlags;

namespace divine {
const char *versionString();
const char *buildDateString();
struct BuildInfo {};
std::ostream &operator<<( std::ostream &o, BuildInfo );
}

#endif
