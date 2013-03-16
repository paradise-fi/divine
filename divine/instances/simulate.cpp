#include <divine/algorithm/simulate.h>
#include <divine/instances/definitions.h>
#include <divine/instances/instantiate.h>

namespace divine {
namespace instantiate {
ALGO_SPEC( Simulate );
#undef ALGO_SPEC

algorithm::Algorithm *selectSimulate( Meta &meta ) {
    return selectGenerator< Algorithm::Simulate, Generator::NotSelected,
           Transform::NotSelected, Store::NotSelected, Visitor::NotSelected,
           Topology::NotSelected, Statistics::Enabled >( meta );
}
}
}
