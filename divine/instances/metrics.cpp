#include <divine/algorithm/metrics.h>
#include <divine/instances/definitions.h>
#include <divine/instances/instantiate.h>

namespace divine {
namespace instantiate {
ALGO_SPEC( Metrics );
#undef ALGO_SPEC

algorithm::Algorithm *selectMetrics( Meta &meta ) {
    return selectGenerator< Algorithm::Metrics, Generator::NotSelected,
           Transform::NotSelected, Store::NotSelected, Visitor::NotSelected,
           Topology::NotSelected, Statistics::Enabled >( meta );
}
}
}
