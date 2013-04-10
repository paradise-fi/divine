#include <divine/algorithm/nested-dfs.h>
#include <divine/instances/instantiate.h>

namespace divine {
namespace instantiate {
ALGO_SPEC( NestedDFS );
#undef ALGO_SPEC

algorithm::Algorithm *selectNDFS( Meta &meta ) {
    return selectGenerator< Algorithm::NestedDFS, Generator::NotSelected,
           Transform::NotSelected, Store::NotSelected, Visitor::NotSelected,
           Topology::NotSelected, Statistics::Enabled >( meta );
}
}

}
