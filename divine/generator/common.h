// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/toolkit/blob.h>
#include <divine/toolkit/hashset.h>
#include <divine/graph/allocator.h>
#include <divine/graph/graph.h>

#ifndef DIVINE_GENERATOR_COMMON_H
#define DIVINE_GENERATOR_COMMON_H

namespace divine {
namespace generator {

using namespace graph;

template< typename _Node >
struct Common : graph::Base< _Node > {
    typedef _Node Node;
};

}
}

#endif
