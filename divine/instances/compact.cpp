#ifdef O_COMPACT
#include <divine/algorithm/compact.h>
#include <divine/instances/definitions.h>
#include <divine/instances/instantiate.h>

namespace divine {
namespace instantiate {
ALGO_SPEC( Compact );
#undef ALGO_SPEC

algorithm::Algorithm *selectCompact( Meta &meta ) {
    return selectGenerator< Algorithm::Compact, Generator::NotSelected,
           Transform::NotSelected, Store::NotSelected, Visitor::NotSelected,
           Topology::NotSelected, Statistics::Enabled >( meta );
}
}
}

#endif
