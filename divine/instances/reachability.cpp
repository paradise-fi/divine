#include <divine/algorithm/reachability.h>
#include <divine/instances/definitions.h>
#include <divine/instances/instantiate.h>

namespace divine {
namespace instantiate {
ALGO_SPEC( Reachability );
#undef ALGO_SPEC

algorithm::Algorithm *selectReachability( Meta &meta ) {
    return selectGenerator< Algorithm::Reachability, Generator::NotSelected,
           Transform::NotSelected, Store::NotSelected, Visitor::NotSelected,
           Topology::NotSelected, Statistics::Enabled >( meta );
}
}
}
