// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/allocator.h>
#include <divine/blob.h>
#include <divine/hashset.h>
#include <divine/graph.h>
#include <divine/algorithm/common.h>

#ifndef DIVINE_GENERATOR_COMMON_H
#define DIVINE_GENERATOR_COMMON_H

namespace divine {
namespace generator {

using namespace graph;

template< typename _Node >
struct Common : graph::Base< _Node > {
    typedef _Node Node;
    /// Default storage for the visited set. May be overriden by subclasses.
    typedef HashSet< Node, algorithm::Hasher, divine::valid< Node >, algorithm::Equal > Table;
};

}
}

#endif
