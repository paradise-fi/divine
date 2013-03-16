#include <divine/algorithm/map.h>
#include <divine/instances/definitions.h>
#include <divine/instances/instantiate.h>

namespace divine {
namespace instantiate {
ALGO_SPEC( Map );
#undef ALGO_SPEC

algorithm::Algorithm *selectMAP( Meta &meta ) {
    return selectGenerator< Algorithm::Map, Generator::NotSelected,
           Transform::NotSelected, Store::NotSelected, Visitor::NotSelected,
           Topology::NotSelected, Statistics::Enabled >( meta );
}
}
}
