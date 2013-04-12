#include <divine/algorithm/owcty.h>
#include <divine/instances/definitions.h>
#include <divine/instances/instantiate.h>

namespace divine {

algorithm::Algorithm *selectOWCTY( Meta &meta ) {
    return selectGenerator< Algorithm::Owcty, Generator::NotSelected,
           Transform::NotSelected, Store::NotSelected, Visitor::NotSelected,
           Topology::NotSelected, Statistics::Enabled >( meta );
}
}
}
