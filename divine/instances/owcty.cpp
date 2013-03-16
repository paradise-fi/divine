#include <divine/algorithm/owcty.h>
#include <divine/instances/definitions.h>
#include <divine/instances/instantiate.h>

namespace divine {
namespace instantiate {
ALGO_SPEC( Owcty );
#undef ALGO_SPEC

algorithm::Algorithm *selectOWCTY( Meta &meta ) {
    return selectGenerator< Algorithm::Owcty, Generator::NotSelected,
           Transform::NotSelected, Store::NotSelected, Visitor::NotSelected,
           Topology::NotSelected, Statistics::Enabled >( meta );
}
}
}
